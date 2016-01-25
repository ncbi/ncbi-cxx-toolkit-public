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
* Author:  Michael Kornbluh, based on template by Pavel Ivanov, NCBI
*
* File Description:
*   Unit test functions in seq_entry_edit.cpp
*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include "unit_test_entry_edit.hpp"

#include <connect/ncbi_pipe.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objtools/edit/edit_exception.hpp>
#include <objtools/edit/seq_entry_edit.hpp>
#include <map>
#include <objtools/unit_test_util/unit_test_util.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// key is processing stage (e.g. "input", "output_expected", etc.)
typedef std::map<string, CFile> TMapTestFiles;

// key is name of test
// value is all the files for that test
typedef std::map<string, TMapTestFiles> TMapTestNameToTestFiles;

// key is function to test (e.g. "divvy")
// value is all the tests for that function
typedef std::map<string, TMapTestNameToTestFiles> TMapFunctionToVecOfTests;

namespace {

    TMapFunctionToVecOfTests s_mapFunctionToVecOfTests;
    CRef<CScope> s_pScope;

    // this loads the files it's given
    // into s_mapFunctionToVecOfTests
    class CFileSorter {
    public:
        CFileSorter(TMapFunctionToVecOfTests * out_pMapFunctionToVecOfTests) : m_pMapFunctionToVecOfTests(out_pMapFunctionToVecOfTests) { }

        void operator()(const CDirEntry & dir_entry) {
            if( ! dir_entry.IsFile() ) {
                return;
            }

            // skip files with unknown extensions
            static const char * arrAllowedExtensions[] = {
                ".asn"
            };
            bool bHasAllowedExtension = false;
            for( size_t ii = 0;
                 ii < sizeof(arrAllowedExtensions)/sizeof(arrAllowedExtensions[0]);
                 ++ii ) 
            {
                if( NStr::EndsWith(dir_entry.GetName(), arrAllowedExtensions[ii]) ){
                    bHasAllowedExtension = true;
                }
            }
            if( ! bHasAllowedExtension ) {
                return;
            }

            CFile file(dir_entry);

            // split the name
            vector<string> tokens;
            NStr::Tokenize(file.GetName(), ".", tokens);

            if( tokens.size() != 4u ) {
                throw std::runtime_error("initialization failed trying to tokenize this file: " + file.GetName());
            }
            
            const string & sFunction = tokens[0];
            const string & sTestName = tokens[1];
            const string & sStage    = tokens[2];
            const string & sSuffix   = tokens[3];
            if( sSuffix != "asn")  {
                cout << "Skipping file with unknown suffix: " << file.GetName() << endl;
                return;
            }

            const bool bInsertSuccessful =
                (*m_pMapFunctionToVecOfTests)[sFunction][sTestName].insert(
                    TMapTestFiles::value_type(sStage, file)).second;

            // this checks for duplicate file names
            if( ! bInsertSuccessful ) {
                throw std::runtime_error("initialization failed: duplicate file name: " + file.GetName() );
            }
        }

    private:
        TMapFunctionToVecOfTests * m_pMapFunctionToVecOfTests;
    };

    // sends the contents of the given file through the C preprocessor
    // and then parses the results as a CSeq_entry.
    // throws exception on error.
    CRef<CSeq_entry> s_ReadAndPreprocessEntry( const string & sFilename )
    {
        ifstream in_file(sFilename.c_str());
        BOOST_REQUIRE(in_file.good());
        CRef<CSeq_entry> pEntry( new CSeq_entry );
        in_file >> MSerial_AsnText >> *pEntry;
        return pEntry;
    }

    bool s_AreSeqEntriesEqualAndPrintIfNot(
        const CSeq_entry & entry1,
        const CSeq_entry & entry2 )
    {
        const bool bEqual = entry1.Equals(entry2);
        if( ! bEqual ) {
            // they're not equal, so print them
            cerr << "These entries should be equal but they aren't: " << endl;
            cerr << "Entry 1: " << MSerial_AsnText << entry1 << endl;
            cerr << "Entry 2: " << MSerial_AsnText << entry2 << endl;
        }
        return bEqual;
    }
}

NCBITEST_AUTO_INIT()
{
    s_pScope.Reset( new CScope(*CObjectManager::GetInstance()) );

    static const vector<string> kEmptyStringVec;

    // initialize the map of which files belong to which test
    CFileSorter file_sorter(&s_mapFunctionToVecOfTests);

    CDir test_cases_dir("./entry_edit_test_cases");
    FindFilesInDir(test_cases_dir,
                   kEmptyStringVec, kEmptyStringVec,
                   file_sorter,
                   (fFF_Default | fFF_Recursive ) );

    // print list of tests found
    cout << "List of tests found and their associated files: " << endl;
    ITERATE( TMapFunctionToVecOfTests, func_to_vec_of_tests_it,
             s_mapFunctionToVecOfTests )
    {
        cout << "FUNC: " << func_to_vec_of_tests_it->first << endl;
        ITERATE( TMapTestNameToTestFiles, test_name_to_files_it,
                 func_to_vec_of_tests_it->second )
        {
            cout << "\tTEST NAME: " << test_name_to_files_it->first << endl;
            ITERATE( TMapTestFiles, test_file_it, test_name_to_files_it->second ) {
                cout << "\t\tSTAGE: " << test_file_it->first << " (file path: " << test_file_it->second.GetPath() << ")" << endl;
            }
        }
    }

}

BOOST_AUTO_TEST_CASE(divvy)
{
    cout << "Testing FUNCTION: DivvyUpAlignments" << endl;
    // test the function DivvyUpAlignments

    TMapTestNameToTestFiles & mapOfTests = s_mapFunctionToVecOfTests["divvy"];

    BOOST_CHECK( ! mapOfTests.empty() );

    NON_CONST_ITERATE( TMapTestNameToTestFiles, test_it, mapOfTests ) {
        const string & sTestName = (test_it->first);
        cout << "Running TEST: " << sTestName << endl;

        TMapTestFiles & test_stage_map = (test_it->second);

        BOOST_REQUIRE( test_stage_map.size() == 2u );

        // pull out the stages
        const CFile & input_file = test_stage_map["input"];
        const CFile & output_expected_file = test_stage_map["output_expected"];
        BOOST_CHECK( input_file.Exists() && output_expected_file.Exists() );

        CRef<CSeq_entry> pInputEntry = s_ReadAndPreprocessEntry( input_file.GetPath() );
        CSeq_entry_Handle input_entry_h = s_pScope->AddTopLevelSeqEntry( *pInputEntry );

        edit::TVecOfSeqEntryHandles vecOfEntries;
        // all direct children are used; we do NOT
        // put the topmost seq-entry into the objmgr
        CSeq_entry_CI direct_child_ci( input_entry_h, CSeq_entry_CI::eNonRecursive );
        for( ; direct_child_ci; ++direct_child_ci ) {
            vecOfEntries.push_back( *direct_child_ci );
        }

        // do the actual function calling
        BOOST_CHECK_NO_THROW(edit::DivvyUpAlignments(vecOfEntries));

        CRef<CSeq_entry> pExpectedOutputEntry = s_ReadAndPreprocessEntry( output_expected_file.GetPath() );

        BOOST_CHECK( s_AreSeqEntriesEqualAndPrintIfNot(
            *pInputEntry, *pExpectedOutputEntry) );

        // to avoid mem leaks, now remove what we have
        BOOST_CHECK_NO_THROW(s_pScope->RemoveTopLevelSeqEntry( input_entry_h ) );
    }
}

BOOST_AUTO_TEST_CASE(SegregateSetsByBioseqList)
{
    cout << "Testing FUNCTION: SegregateSetsByBioseqList" << endl;

    TMapTestNameToTestFiles & mapOfTests = s_mapFunctionToVecOfTests["segregate"];

    BOOST_CHECK( ! mapOfTests.empty() );

    NON_CONST_ITERATE( TMapTestNameToTestFiles, test_it, mapOfTests ) {
        const string & sTestName = (test_it->first);
        cout << "Running TEST: " << sTestName << endl;

        TMapTestFiles & test_stage_map = (test_it->second);

        BOOST_REQUIRE( test_stage_map.size() == 2u );

        // pull out the stages
        const CFile & input_entry_file = test_stage_map["input_entry"];
        const CFile & output_expected_file = test_stage_map["output_expected"];

        CRef<CSeq_entry> pInputEntry = s_ReadAndPreprocessEntry( input_entry_file.GetPath() );
        CRef<CSeq_entry> pOutputExpectedEntry = s_ReadAndPreprocessEntry( output_expected_file.GetPath() );

        CSeq_entry_Handle entry_h = s_pScope->AddTopLevelSeqEntry(*pInputEntry);
        CSeq_entry_Handle expected_entry_h = s_pScope->AddTopLevelSeqEntry(*pOutputExpectedEntry);

        // load bioseq_handles with the bioseqs we want to segregate
        CScope::TBioseqHandles bioseq_handles;
        CBioseq_CI bioseq_ci( entry_h );
        for( ; bioseq_ci; ++bioseq_ci ) {
            // see if the bioseq has the user object that marks
            // it as one of the bioseqs to segregate
            CSeqdesc_CI user_desc_ci( *bioseq_ci, CSeqdesc::e_User );
            for( ; user_desc_ci; ++user_desc_ci ) {
                if( FIELD_EQUALS( user_desc_ci->GetUser(), Class,
                                  "objtools.edit.unit_test.segregate") )
                {
                    bioseq_handles.push_back( *bioseq_ci );
                    break;
                }
            }
        }

        BOOST_CHECK_NO_THROW(
            edit::SegregateSetsByBioseqList( entry_h, bioseq_handles ));

        // check if matches expected
        BOOST_CHECK( s_AreSeqEntriesEqualAndPrintIfNot(
             *entry_h.GetCompleteSeq_entry(),
             *expected_entry_h.GetCompleteSeq_entry()) );

        BOOST_CHECK_NO_THROW( s_pScope->RemoveTopLevelSeqEntry(entry_h) );
        BOOST_CHECK_NO_THROW( s_pScope->RemoveTopLevelSeqEntry(expected_entry_h) );
    }
}


void CheckLocalId(const CBioseq& seq, const string& expected)
{
    if (!seq.IsSetDescr()) {
        BOOST_CHECK_EQUAL("No descriptors", "Expected descriptors");
        return;
    }
    int num_user_descriptors_found = 0;
    ITERATE(CBioseq::TDescr::Tdata, it, seq.GetDescr().Get()) {
        if ((*it)->IsUser()) {
            const CUser_object& usr = (*it)->GetUser();
            BOOST_CHECK_EQUAL(usr.GetObjectType(), CUser_object::eObjectType_OriginalId);
            BOOST_CHECK_EQUAL(usr.GetData()[0]->GetLabel().GetStr(), "LocalId");
            BOOST_CHECK_EQUAL(usr.GetData()[0]->GetData().GetStr(), expected);
            num_user_descriptors_found++;;
        }
    }
    BOOST_CHECK_EQUAL(num_user_descriptors_found, 1);

}


BOOST_AUTO_TEST_CASE(FixCollidingIds)
{
    CRef<CSeq_entry> entry1 = unit_test_util::BuildGoodSeq();
    unit_test_util::SetDrosophila_melanogaster(entry1);
    CRef<CSeq_entry> entry2 = unit_test_util::BuildGoodSeq();
    unit_test_util::SetSebaea_microphylla(entry2);
    CRef<CSeq_entry> set_entry(new CSeq_entry());
    set_entry->SetSet().SetClass(CBioseq_set::eClass_phy_set);
    set_entry->SetSet().SetSeq_set().push_back(entry1);
    set_entry->SetSet().SetSeq_set().push_back(entry2);
    edit::AddLocalIdUserObjects(*set_entry);
    CheckLocalId(entry1->GetSeq(), "good");
    CheckLocalId(entry2->GetSeq(), "good");

    set_entry->ReassignConflictingIds();
    BOOST_CHECK_EQUAL(entry1->GetSeq().GetId().front()->GetLocal().GetStr(), "good");
    BOOST_CHECK_EQUAL(entry2->GetSeq().GetId().front()->GetLocal().GetStr(), "good_1");
    BOOST_CHECK_EQUAL(true, edit::HasRepairedIDs(*set_entry));
    CRef<CSeq_entry> entry3 = unit_test_util::BuildGoodSeq();
    set_entry->SetSet().SetSeq_set().push_back(entry3);
    edit::AddLocalIdUserObjects(*set_entry);
    CheckLocalId(entry1->GetSeq(), "good");
    CheckLocalId(entry2->GetSeq(), "good");
    CheckLocalId(entry3->GetSeq(), "good");
    set_entry->ReassignConflictingIds();
    BOOST_CHECK_EQUAL(entry1->GetSeq().GetId().front()->GetLocal().GetStr(), "good");
    BOOST_CHECK_EQUAL(entry2->GetSeq().GetId().front()->GetLocal().GetStr(), "good_1");
    BOOST_CHECK_EQUAL(entry3->GetSeq().GetId().front()->GetLocal().GetStr(), "good_2");
    BOOST_CHECK_EQUAL(true, edit::HasRepairedIDs(*set_entry));

    CRef<CSeq_entry> good_set = unit_test_util::BuildGoodEcoSet();
    BOOST_CHECK_EQUAL(false, edit::HasRepairedIDs(*good_set));
    edit::AddLocalIdUserObjects(*good_set);
    BOOST_CHECK_EQUAL(false, edit::HasRepairedIDs(*good_set));
    good_set->ReassignConflictingIds();
    BOOST_CHECK_EQUAL(false, edit::HasRepairedIDs(*good_set));

}


void s_CheckSeg(const CDelta_seq& ds, bool expect_gap, size_t expect_length)
{
    BOOST_CHECK_EQUAL(ds.IsLiteral(), true);
    BOOST_CHECK_EQUAL(ds.GetLiteral().GetSeq_data().IsGap(), expect_gap);
    BOOST_CHECK_EQUAL(ds.GetLiteral().GetLength(), expect_length);
}


CRef<CSeq_entry> MakeEntryForDeltaConversion(vector<string> segs)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    CSeq_inst& inst = entry->SetSeq().SetInst();
    string seq_str = "";
    ITERATE(vector<string>, it, segs) {
        seq_str += *it;
    }
    size_t orig_len = seq_str.length();
    inst.SetSeq_data().SetIupacna().Set(seq_str);
    inst.SetLength(orig_len);

    CRef<CSeq_id> id(new CSeq_id());
    id->Assign(*(entry->GetSeq().GetId().front()));
   
    CRef<CSeq_annot> annot(new CSeq_annot());
    entry->SetSeq().SetAnnot().push_back(annot);
    // first feature covers entire sequence
    CRef<CSeq_feat> f1(new CSeq_feat());
    f1->SetData().SetImp().SetKey("misc_feature");
    f1->SetLocation().SetInt().SetId().Assign(*id);
    f1->SetLocation().SetInt().SetFrom(0);
    f1->SetLocation().SetInt().SetTo(entry->GetSeq().GetLength() - 1);
    annot->SetData().SetFtable().push_back(f1);

    // second feature is coding region, code break is after gap of unknown length
    CRef<CSeq_feat> f2 (new CSeq_feat());
    CRef<CCode_break> brk(new CCode_break());
    brk->SetLoc().SetInt().SetId().Assign(*id);
    brk->SetLoc().SetInt().SetFrom(54);
    brk->SetLoc().SetInt().SetTo(56);
    f2->SetData().SetCdregion().SetCode_break().push_back(brk);
    f2->SetLocation().SetInt().SetId().Assign(*id);
    f2->SetLocation().SetInt().SetFrom(0);
    f2->SetLocation().SetInt().SetTo(entry->GetSeq().GetLength() - 1);
    annot->SetData().SetFtable().push_back(f2);

    // third feature is tRNA, code break is before gap of unknown length, mixed location
    CRef<CSeq_feat> f3 (new CSeq_feat());
    CSeq_interval& interval = f3->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt();
    interval.SetFrom(6);
    interval.SetTo(8);
    interval.SetId().Assign(*id);
    f3->SetData().SetRna().SetType(CRNA_ref::eType_tRNA);

    //make mix location that does not include gaps
    size_t pos = 0;
    ITERATE(vector<string>, it, segs) {
        if (NStr::Find(*it, "N") == string::npos) {
            CRef<CSeq_loc> l1(new CSeq_loc());
            l1->SetInt().SetFrom(pos);
            l1->SetInt().SetTo(pos +  it->length() - 1);
            l1->SetInt().SetId().Assign(*id);
            f3->SetLocation().SetMix().Set().push_back(l1);
        }
        pos += it->length();
    }
    return entry;
}


void s_IntervalsMatchGaps(const CSeq_loc& loc, const CSeq_inst& inst)
{
    BOOST_CHECK_EQUAL(loc.Which(), CSeq_loc::e_Mix);
    BOOST_CHECK_EQUAL(inst.GetRepr(), CSeq_inst::eRepr_delta);
    if (loc.Which() != CSeq_loc::e_Mix || inst.GetRepr() != CSeq_inst::eRepr_delta) {
        return;
    }

    CSeq_loc_CI ci(loc);
    CSeq_ext::TDelta::Tdata::const_iterator ds_it = inst.GetExt().GetDelta().Get().begin();
    size_t pos = 0;
    while ((ds_it != inst.GetExt().GetDelta().Get().end()) && ci) {
        const CSeq_literal& lit = (*ds_it)->GetLiteral();
        if (lit.IsSetSeq_data() && !lit.GetSeq_data().IsGap()) {
            BOOST_CHECK_EQUAL(ci.GetRange().GetFrom(), pos);
            BOOST_CHECK_EQUAL(ci.GetRange().GetTo(), pos + lit.GetLength() - 1);
            ++ci;
        }
        pos += lit.GetLength();
        ds_it++;
    }
}


BOOST_AUTO_TEST_CASE(Test_ConvertRawToDeltaByNs) 
{
    vector<string> segs;
    segs.push_back("AAAAAAAAAAAAAAAAAAAAAAAA");
    segs.push_back("NNNNNNNNNNNNNNN");
    segs.push_back("TTTTTTTT");
    segs.push_back("NNNNN");
    segs.push_back("TTTTTTTTTT");

    vector<size_t> lens;
    vector<bool> is_gap;
    ITERATE(vector<string>, it, segs) {
        if (NStr::Find(*it, "N") != string::npos) {
            is_gap.push_back(true);
            if ((*it).length() == 5) {
                lens.push_back(100);
            } else {
                lens.push_back((*it).length());
            }
        } else {
            is_gap.push_back(false);
            lens.push_back((*it).length());
        }        
    }

    CRef<CSeq_entry> entry = MakeEntryForDeltaConversion (segs);
    CSeq_inst& inst = entry->SetSeq().SetInst();
    size_t orig_len = inst.GetLength();

    // This should convert the first run of Ns (15) to a gap of known length
    // and the second run of Ns (5) to a gap of unknown length
    edit::ConvertRawToDeltaByNs(inst, 5, 5, 10, -1, true);
    edit::NormalizeUnknownLengthGaps(inst);
    BOOST_CHECK_EQUAL(inst.GetRepr(), CSeq_inst::eRepr_delta);
    BOOST_CHECK_EQUAL(inst.GetExt().GetDelta().Get().size(), 5);
    BOOST_CHECK_EQUAL(inst.GetLength(), orig_len - 5 + 100);
    CSeq_ext::TDelta::Tdata::const_iterator ds_it = inst.GetExt().GetDelta().Get().begin();
    vector<bool>::iterator is_gap_it = is_gap.begin();
    vector<size_t>::iterator len_it = lens.begin();
    while (ds_it != inst.GetExt().GetDelta().Get().end()) {
        s_CheckSeg(**ds_it, *is_gap_it, *len_it);
        ds_it++;
        is_gap_it++;
        len_it++;
    }


    edit::SetLinkageType(inst.SetExt(), CSeq_gap::eType_short_arm);
    ITERATE(CSeq_ext::TDelta::Tdata, it, inst.GetExt().GetDelta().Get()) {
        if ((*it)->GetLiteral().GetSeq_data().IsGap()) {
            BOOST_CHECK_EQUAL((*it)->GetLiteral().GetSeq_data().GetGap().GetType(), CSeq_gap::eType_short_arm);
        }
    }

    edit::SetLinkageType(inst.SetExt(), CSeq_gap::eType_scaffold);
    ITERATE(CSeq_ext::TDelta::Tdata, it, inst.GetExt().GetDelta().Get()) {
        if ((*it)->GetLiteral().GetSeq_data().IsGap()) {
            const CSeq_gap& gap = (*it)->GetLiteral().GetSeq_data().GetGap();
            BOOST_CHECK_EQUAL(gap.GetType(), CSeq_gap::eType_scaffold);
            BOOST_CHECK_EQUAL(gap.GetLinkage(), CSeq_gap::eLinkage_linked);
            BOOST_CHECK_EQUAL(gap.GetLinkage_evidence().front()->GetType(), CLinkage_evidence::eType_unspecified);
        }
    }

    edit::SetLinkageTypeLinkedRepeat(inst.SetExt(), CLinkage_evidence::eType_paired_ends);
    ITERATE(CSeq_ext::TDelta::Tdata, it, inst.GetExt().GetDelta().Get()) {
        if ((*it)->GetLiteral().GetSeq_data().IsGap()) {
            const CSeq_gap& gap = (*it)->GetLiteral().GetSeq_data().GetGap();
            BOOST_CHECK_EQUAL(gap.GetType(), CSeq_gap::eType_repeat);
            BOOST_CHECK_EQUAL(gap.GetLinkage(), CSeq_gap::eLinkage_linked);
            BOOST_CHECK_EQUAL(gap.GetLinkage_evidence().front()->GetType(), CLinkage_evidence::eType_paired_ends);
        }
    }

    edit::SetLinkageType(inst.SetExt(), CSeq_gap::eType_centromere);
    ITERATE(CSeq_ext::TDelta::Tdata, it, inst.GetExt().GetDelta().Get()) {
        if ((*it)->GetLiteral().GetSeq_data().IsGap()) {
            const CSeq_gap& gap = (*it)->GetLiteral().GetSeq_data().GetGap();
            BOOST_CHECK_EQUAL(gap.GetType(), CSeq_gap::eType_centromere);
            BOOST_CHECK_EQUAL(gap.IsSetLinkage(), false);
            BOOST_CHECK_EQUAL(gap.IsSetLinkage_evidence(), false);
        }        
    }


    entry = MakeEntryForDeltaConversion (segs);


    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);
    CBioseq_Handle bsh = seh.GetSeq();
    edit::ConvertRawToDeltaByNs(bsh, 5, 5, 10, -1, true);

    CFeat_CI fi(bsh);
    while (fi) {
        BOOST_CHECK_EQUAL(fi->GetLocation().GetInt().GetFrom(), 0);
        BOOST_CHECK_EQUAL(fi->GetLocation().GetInt().GetTo(), entry->GetSeq().GetInst().GetLength() - 1);
        if (fi->GetData().IsCdregion()) {
            const CSeq_interval& interval = fi->GetData().GetCdregion().GetCode_break().front()->GetLoc().GetInt();
            BOOST_CHECK_EQUAL(interval.GetFrom(), 149);
            BOOST_CHECK_EQUAL(interval.GetTo(), 151);
        } else if (fi->GetData().GetSubtype() == CSeqFeatData::eSubtype_tRNA) {
            s_IntervalsMatchGaps(fi->GetLocation(), entry->GetSeq().GetInst());
            const CSeq_interval& interval = fi->GetData().GetRna().GetExt().GetTRNA().GetAnticodon().GetInt();
            BOOST_CHECK_EQUAL(interval.GetFrom(), 6);
            BOOST_CHECK_EQUAL(interval.GetTo(), 8);
        }
        ++fi;
    }

}


static bool s_FindLocalId(const CBioseq_Handle& bsh, const string& sLocalid)
{
    const CBioseq_Handle::TId& ids = bsh.GetId();
    ITERATE( CBioseq_Handle::TId, id_itr, ids ) {
        const CSeq_id_Handle& id_handle = *id_itr;
        CConstRef<CSeq_id> id = id_handle.GetSeqIdOrNull();
        if ( !id ) {
            continue;
        }

        if ( id->IsLocal() ) {
            // Found localid
            const CObject_id& localid = id->GetLocal();
            if ( localid.IsStr() ) {
                // Are the string values equal?
                return localid.GetStr() == sLocalid;
            }
        }
    }

    return false;
}


BOOST_AUTO_TEST_CASE(TrimSeqData)
{
    cout << "Testing FUNCTION: TrimSeqData" << endl;

    TMapTestNameToTestFiles & mapOfTests = s_mapFunctionToVecOfTests["trim_seq_data"];

    BOOST_CHECK( ! mapOfTests.empty() );

    NON_CONST_ITERATE( TMapTestNameToTestFiles, test_it, mapOfTests ) {
        const string & sTestName = (test_it->first);
        cout << "Running TEST: " << sTestName << endl;

        TMapTestFiles & test_stage_map = (test_it->second);

        BOOST_REQUIRE( test_stage_map.size() == 2u );

        // Get the input/output files
        const CFile & input_entry_file = test_stage_map["input_entry"];
        const CFile & output_expected_file = test_stage_map["output_expected"];

        CRef<CSeq_entry> pInputEntry = s_ReadAndPreprocessEntry( input_entry_file.GetPath() );
        CRef<CSeq_entry> pOutputExpectedEntry = s_ReadAndPreprocessEntry( output_expected_file.GetPath() );

        CSeq_entry_Handle entry_h = s_pScope->AddTopLevelSeqEntry(*pInputEntry);
        CSeq_entry_Handle expected_entry_h = s_pScope->AddTopLevelSeqEntry(*pOutputExpectedEntry);

        // Find the bioseq that we will trim
        CBioseq_CI bioseq_ci( entry_h );
        for( ; bioseq_ci; ++bioseq_ci ) {
            const CBioseq_Handle& bsh = *bioseq_ci;
            if (s_FindLocalId(bsh, "DH5a-357R-3")) {
                // Create the cuts from known vector contamination
                // Seqid "DH5a-357R-3" has two vector locations
                edit::TRange cut1(600, 643);
                edit::TRange cut2(644, 646);
                edit::TCuts cuts;
                cuts.push_back(cut1);
                cuts.push_back(cut2);

                // Sort the cuts
                edit::TCuts sorted_cuts;
                BOOST_CHECK_NO_THROW(edit::GetSortedCuts( bsh, cuts, sorted_cuts ));

                // Create a copy of inst
                CRef<CSeq_inst> copy_inst(new CSeq_inst());
                copy_inst->Assign(bsh.GetInst());

                // Make changes to the inst copy 
                BOOST_CHECK_NO_THROW(edit::TrimSeqData( bsh, copy_inst, sorted_cuts ));

                // Update the input seqentry with the changes
                bsh.GetEditHandle().SetInst(*copy_inst);

                break;
            }
        }

        // Are the changes what we expect?
        BOOST_CHECK( s_AreSeqEntriesEqualAndPrintIfNot(
             *entry_h.GetCompleteSeq_entry(),
             *expected_entry_h.GetCompleteSeq_entry()) );

        BOOST_CHECK_NO_THROW( s_pScope->RemoveTopLevelSeqEntry(entry_h) );
        BOOST_CHECK_NO_THROW( s_pScope->RemoveTopLevelSeqEntry(expected_entry_h) );
    }
}


BOOST_AUTO_TEST_CASE(TrimSeqGraph)
{
    cout << "Testing FUNCTION: TrimSeqGraph" << endl;

    TMapTestNameToTestFiles & mapOfTests = s_mapFunctionToVecOfTests["trim_seq_graph"];

    BOOST_CHECK( ! mapOfTests.empty() );

    NON_CONST_ITERATE( TMapTestNameToTestFiles, test_it, mapOfTests ) {
        const string & sTestName = (test_it->first);
        cout << "Running TEST: " << sTestName << endl;

        TMapTestFiles & test_stage_map = (test_it->second);

        BOOST_REQUIRE( test_stage_map.size() == 2u );

        // Get the input/output files
        const CFile & input_entry_file = test_stage_map["input_entry"];
        const CFile & output_expected_file = test_stage_map["output_expected"];

        CRef<CSeq_entry> pInputEntry = s_ReadAndPreprocessEntry( input_entry_file.GetPath() );
        CRef<CSeq_entry> pOutputExpectedEntry = s_ReadAndPreprocessEntry( output_expected_file.GetPath() );

        CSeq_entry_Handle entry_h = s_pScope->AddTopLevelSeqEntry(*pInputEntry);
        CSeq_entry_Handle expected_entry_h = s_pScope->AddTopLevelSeqEntry(*pOutputExpectedEntry);

        // Find the bioseq(s) that we will trim
        CBioseq_CI bioseq_ci( entry_h );
        for( ; bioseq_ci; ++bioseq_ci ) {
            const CBioseq_Handle& bsh = *bioseq_ci;

            if (s_FindLocalId(bsh, "cont1.86")) {
                // Create the cuts from known vector contamination
                // Seqid "cont1.86" has vector
                edit::TRange cut1(913, 948);
                edit::TCuts cuts;
                cuts.push_back(cut1);

                // Sort the cuts
                edit::TCuts sorted_cuts;
                BOOST_CHECK_NO_THROW(edit::GetSortedCuts( bsh, cuts, sorted_cuts ));

                // Iterate over bioseq graphs
                SAnnotSelector graph_sel(CSeq_annot::C_Data::e_Graph);
                CGraph_CI graph_ci(bsh, graph_sel);
                for (; graph_ci; ++graph_ci) {
                    // Only certain types of graphs are supported.
                    // See C Toolkit function GetGraphsProc in api/sqnutil2.c
                    const CMappedGraph& graph = *graph_ci;
                    if ( graph.IsSetTitle() && 
                         (NStr::CompareNocase( graph.GetTitle(), "Phrap Quality" ) == 0 ||
                          NStr::CompareNocase( graph.GetTitle(), "Phred Quality" ) == 0 ||
                          NStr::CompareNocase( graph.GetTitle(), "Gap4" ) == 0) )
                    {
                        // Make a copy of the graph
                        CRef<CSeq_graph> copy_graph(new CSeq_graph());
                        copy_graph->Assign(graph.GetOriginalGraph());

                        // Modify the copy of the graph
                        BOOST_CHECK_NO_THROW(edit::TrimSeqGraph(bsh, copy_graph, sorted_cuts));

                        // Update the original graph with the modified copy
                        graph.GetSeq_graph_Handle().Replace(*copy_graph);
                    }
                }

                // Create a copy of inst
                CRef<CSeq_inst> copy_inst(new CSeq_inst());
                copy_inst->Assign(bsh.GetInst());

                // Make changes to the inst copy 
                BOOST_CHECK_NO_THROW(edit::TrimSeqData( bsh, copy_inst, sorted_cuts ));

                // Update the input seqentry with the changes
                bsh.GetEditHandle().SetInst(*copy_inst);
            }

            if (s_FindLocalId(bsh, "cont1.95")) {
                // Create the cuts from known vector contamination
                // Seqid "cont1.95" has vector
                edit::TRange cut1(0, 30);
                edit::TCuts cuts;
                cuts.push_back(cut1);

                // Sort the cuts
                edit::TCuts sorted_cuts;
                BOOST_CHECK_NO_THROW(edit::GetSortedCuts( bsh, cuts, sorted_cuts ));

                // Iterate over bioseq graphs
                SAnnotSelector graph_sel(CSeq_annot::C_Data::e_Graph);
                CGraph_CI graph_ci(bsh, graph_sel);
                for (; graph_ci; ++graph_ci) {
                    // Only certain types of graphs are supported.
                    // See C Toolkit function GetGraphsProc in api/sqnutil2.c
                    const CMappedGraph& graph = *graph_ci;
                    if ( graph.IsSetTitle() && 
                         (NStr::CompareNocase( graph.GetTitle(), "Phrap Quality" ) == 0 ||
                          NStr::CompareNocase( graph.GetTitle(), "Phred Quality" ) == 0 ||
                          NStr::CompareNocase( graph.GetTitle(), "Gap4" ) == 0) )
                    {
                        // Make a copy of the graph
                        CRef<CSeq_graph> copy_graph(new CSeq_graph());
                        copy_graph->Assign(graph.GetOriginalGraph());

                        // Modify the copy of the graph
                        BOOST_CHECK_NO_THROW(edit::TrimSeqGraph(bsh, copy_graph, sorted_cuts));

                        // Update the original graph with the modified copy
                        graph.GetSeq_graph_Handle().Replace(*copy_graph);
                    }
                }

                // Create a copy of inst
                CRef<CSeq_inst> copy_inst(new CSeq_inst());
                copy_inst->Assign(bsh.GetInst());

                // Make changes to the inst copy 
                BOOST_CHECK_NO_THROW(edit::TrimSeqData( bsh, copy_inst, sorted_cuts ));

                // Update the input seqentry with the changes
                bsh.GetEditHandle().SetInst(*copy_inst);
            }
        }

        // Are the changes what we expect?
        BOOST_CHECK( s_AreSeqEntriesEqualAndPrintIfNot(
             *entry_h.GetCompleteSeq_entry(),
             *expected_entry_h.GetCompleteSeq_entry()) );

        BOOST_CHECK_NO_THROW( s_pScope->RemoveTopLevelSeqEntry(entry_h) );
        BOOST_CHECK_NO_THROW( s_pScope->RemoveTopLevelSeqEntry(expected_entry_h) );
    }
}


BOOST_AUTO_TEST_CASE(TrimSeqAlign)
{
    cout << "Testing FUNCTION: TrimSeqAlign" << endl;

    TMapTestNameToTestFiles & mapOfTests = s_mapFunctionToVecOfTests["trim_seq_align"];

    BOOST_CHECK( ! mapOfTests.empty() );

    NON_CONST_ITERATE( TMapTestNameToTestFiles, test_it, mapOfTests ) {
        const string & sTestName = (test_it->first);
        cout << "Running TEST: " << sTestName << endl;

        TMapTestFiles & test_stage_map = (test_it->second);

        BOOST_REQUIRE( test_stage_map.size() == 2u );

        // Get the input/output files
        const CFile & input_entry_file = test_stage_map["input_entry"];
        const CFile & output_expected_file = test_stage_map["output_expected"];

        CRef<CSeq_entry> pInputEntry = s_ReadAndPreprocessEntry( input_entry_file.GetPath() );
        CRef<CSeq_entry> pOutputExpectedEntry = s_ReadAndPreprocessEntry( output_expected_file.GetPath() );

        CSeq_entry_Handle entry_h = s_pScope->AddTopLevelSeqEntry(*pInputEntry);
        CSeq_entry_Handle expected_entry_h = s_pScope->AddTopLevelSeqEntry(*pOutputExpectedEntry);

        // Find the bioseq(s) that we will trim
        CBioseq_CI bioseq_ci( entry_h );
        for( ; bioseq_ci; ++bioseq_ci ) {
            const CBioseq_Handle& bsh = *bioseq_ci;

            if (s_FindLocalId(bsh, "CBS118702")) {
                // Create the cuts from known vector contamination
                // Seqid "CBS118702" has vector
                edit::TRange cut1(479, 502);
                edit::TCuts cuts;
                cuts.push_back(cut1);

                // Sort the cuts
                edit::TCuts sorted_cuts;
                BOOST_CHECK_NO_THROW(edit::GetSortedCuts( bsh, cuts, sorted_cuts ));

                // Iterate over bioseq alignments
                SAnnotSelector align_sel(CSeq_annot::C_Data::e_Align);
                CAlign_CI align_ci(bsh, align_sel);
                for (; align_ci; ++align_ci) {
                    // Only DENSEG type is supported
                    const CSeq_align& align = *align_ci;
                    if ( align.CanGetSegs() && 
                         align.GetSegs().Which() == CSeq_align::C_Segs::e_Denseg )
                    {
                        // Make sure mandatory fields are present in the denseg
                        const CDense_seg& denseg = align.GetSegs().GetDenseg();
                        if (! (denseg.CanGetDim() && denseg.CanGetNumseg() && 
                               denseg.CanGetIds() && denseg.CanGetStarts() &&
                               denseg.CanGetLens()) )
                        {
                            continue;
                        }

                        // Make a copy of the alignment
                        CRef<CSeq_align> copy_align(new CSeq_align());
                        copy_align->Assign(align_ci.GetOriginalSeq_align());

                        // Modify the copy of the alignment
                        BOOST_CHECK_NO_THROW(edit::TrimSeqAlign(bsh, copy_align, sorted_cuts));

                        // Update the original alignment with the modified copy
                        align_ci.GetSeq_align_Handle().Replace(*copy_align);
                    }
                }

                // Create a copy of inst
                CRef<CSeq_inst> copy_inst(new CSeq_inst());
                copy_inst->Assign(bsh.GetInst());

                // Make changes to the inst copy 
                BOOST_CHECK_NO_THROW(edit::TrimSeqData( bsh, copy_inst, sorted_cuts ));

                // Update the input seqentry with the changes
                bsh.GetEditHandle().SetInst(*copy_inst);
            }

            if (s_FindLocalId(bsh, "CBS124120")) {
                // Create the cuts from known vector contamination
                // Seqid "CBS124120" has vector
                edit::TRange cut1(479, 502);
                edit::TCuts cuts;
                cuts.push_back(cut1);

                // Sort the cuts
                edit::TCuts sorted_cuts;
                BOOST_CHECK_NO_THROW(edit::GetSortedCuts( bsh, cuts, sorted_cuts ));

                // Iterate over bioseq alignments
                SAnnotSelector align_sel(CSeq_annot::C_Data::e_Align);
                CAlign_CI align_ci(bsh, align_sel);
                for (; align_ci; ++align_ci) {
                    // Only DENSEG type is supported
                    const CSeq_align& align = *align_ci;
                    if ( align.CanGetSegs() && 
                         align.GetSegs().Which() == CSeq_align::C_Segs::e_Denseg )
                    {
                        // Make sure mandatory fields are present in the denseg
                        const CDense_seg& denseg = align.GetSegs().GetDenseg();
                        if (! (denseg.CanGetDim() && denseg.CanGetNumseg() && 
                               denseg.CanGetIds() && denseg.CanGetStarts() &&
                               denseg.CanGetLens()) )
                        {
                            continue;
                        }

                        // Make a copy of the alignment
                        CRef<CSeq_align> copy_align(new CSeq_align());
                        copy_align->Assign(align_ci.GetOriginalSeq_align());

                        // Modify the copy of the alignment
                        BOOST_CHECK_NO_THROW(edit::TrimSeqAlign(bsh, copy_align, sorted_cuts));

                        // Update the original alignment with the modified copy
                        align_ci.GetSeq_align_Handle().Replace(*copy_align);
                    }
                }

                // Create a copy of inst
                CRef<CSeq_inst> copy_inst(new CSeq_inst());
                copy_inst->Assign(bsh.GetInst());

                // Make changes to the inst copy 
                BOOST_CHECK_NO_THROW(edit::TrimSeqData( bsh, copy_inst, sorted_cuts ));

                // Update the input seqentry with the changes
                bsh.GetEditHandle().SetInst(*copy_inst);
            }

            if (s_FindLocalId(bsh, "CBS534.83")) {
                // Create the cuts from known vector contamination
                // Seqid "CBS534.83" has vector
                edit::TRange cut1(479, 502);
                edit::TCuts cuts;
                cuts.push_back(cut1);

                // Sort the cuts
                edit::TCuts sorted_cuts;
                BOOST_CHECK_NO_THROW(edit::GetSortedCuts( bsh, cuts, sorted_cuts ));

                // Iterate over bioseq alignments
                SAnnotSelector align_sel(CSeq_annot::C_Data::e_Align);
                CAlign_CI align_ci(bsh, align_sel);
                for (; align_ci; ++align_ci) {
                    // Only DENSEG type is supported
                    const CSeq_align& align = *align_ci;
                    if ( align.CanGetSegs() && 
                         align.GetSegs().Which() == CSeq_align::C_Segs::e_Denseg )
                    {
                        // Make sure mandatory fields are present in the denseg
                        const CDense_seg& denseg = align.GetSegs().GetDenseg();
                        if (! (denseg.CanGetDim() && denseg.CanGetNumseg() && 
                               denseg.CanGetIds() && denseg.CanGetStarts() &&
                               denseg.CanGetLens()) )
                        {
                            continue;
                        }

                        // Make a copy of the alignment
                        CRef<CSeq_align> copy_align(new CSeq_align());
                        copy_align->Assign(align_ci.GetOriginalSeq_align());

                        // Modify the copy of the alignment
                        BOOST_CHECK_NO_THROW(edit::TrimSeqAlign(bsh, copy_align, sorted_cuts));

                        // Update the original alignment with the modified copy
                        align_ci.GetSeq_align_Handle().Replace(*copy_align);
                    }
                }

                // Create a copy of inst
                CRef<CSeq_inst> copy_inst(new CSeq_inst());
                copy_inst->Assign(bsh.GetInst());

                // Make changes to the inst copy 
                BOOST_CHECK_NO_THROW(edit::TrimSeqData( bsh, copy_inst, sorted_cuts ));

                // Update the input seqentry with the changes
                bsh.GetEditHandle().SetInst(*copy_inst);
            }
        }

        // Are the changes what we expect?
        BOOST_CHECK( s_AreSeqEntriesEqualAndPrintIfNot(
             *entry_h.GetCompleteSeq_entry(),
             *expected_entry_h.GetCompleteSeq_entry()) );

        BOOST_CHECK_NO_THROW( s_pScope->RemoveTopLevelSeqEntry(entry_h) );
        BOOST_CHECK_NO_THROW( s_pScope->RemoveTopLevelSeqEntry(expected_entry_h) );
    }
}


BOOST_AUTO_TEST_CASE(TrimSeqFeat_Featured_Deleted)
{
    cout << "Testing FUNCTION: TrimSeqFeat - cdregion feature was completely deleted" << endl;

    TMapTestNameToTestFiles & mapOfTests = s_mapFunctionToVecOfTests["trim_seq_feat_feature_deleted"];

    BOOST_CHECK( ! mapOfTests.empty() );

    NON_CONST_ITERATE( TMapTestNameToTestFiles, test_it, mapOfTests ) {
        const string & sTestName = (test_it->first);
        cout << "Running TEST: " << sTestName << endl;

        TMapTestFiles & test_stage_map = (test_it->second);

        BOOST_REQUIRE( test_stage_map.size() == 2u );

        // Get the input/output files
        const CFile & input_entry_file = test_stage_map["input_entry"];
        const CFile & output_expected_file = test_stage_map["output_expected"];

        CRef<CSeq_entry> pInputEntry = s_ReadAndPreprocessEntry( input_entry_file.GetPath() );
        CRef<CSeq_entry> pOutputExpectedEntry = s_ReadAndPreprocessEntry( output_expected_file.GetPath() );

        CSeq_entry_Handle entry_h = s_pScope->AddTopLevelSeqEntry(*pInputEntry);
        CSeq_entry_Handle expected_entry_h = s_pScope->AddTopLevelSeqEntry(*pOutputExpectedEntry);

        // Find the bioseq(s) that we will trim
        CBioseq_CI bioseq_ci( entry_h );
        for( ; bioseq_ci; ++bioseq_ci ) {
            const CBioseq_Handle& bsh = *bioseq_ci;

            if (s_FindLocalId(bsh, "Seq53")) {
                // Create the cuts from known vector contamination
                // Seqid "Seq53" has vector
                edit::TRange cut1(0, 2205);
                edit::TCuts cuts;
                cuts.push_back(cut1);

                // Sort the cuts
                edit::TCuts sorted_cuts;
                BOOST_CHECK_NO_THROW(edit::GetSortedCuts( bsh, cuts, sorted_cuts ));

                // Iterate over bioseq features
                SAnnotSelector feat_sel(CSeq_annot::C_Data::e_Ftable);
                CFeat_CI feat_ci(bsh, feat_sel);
                for (; feat_ci; ++feat_ci) {
                    // Make a copy of the feature
                    CRef<CSeq_feat> copy_feat(new CSeq_feat());
                    copy_feat->Assign(feat_ci->GetOriginalFeature());

                    // Detect complete deletions of feature
                    bool bFeatureDeleted = false;

                    // Detect case where feature was not deleted but merely trimmed
                    bool bFeatureTrimmed = false;

                    // Modify the copy of the feature
                    BOOST_CHECK_NO_THROW(edit::TrimSeqFeat(copy_feat, sorted_cuts, bFeatureDeleted, bFeatureTrimmed));

                    if (bFeatureDeleted) {
                        // Delete the feature
                        // If the feature was a cdregion, delete the protein and
                        // renormalize the nuc-prot set
                        BOOST_CHECK_NO_THROW(edit::DeleteProteinAndRenormalizeNucProtSet(*feat_ci));
                    }
                    else 
                    if (bFeatureTrimmed) {
                        // Further modify the copy of the feature

                        // Not testing AdjustCdregionFrame() and RetranslateCdregion()
                        // in this unit test.  See next unit test.

                        // Update the original feature with the modified copy
                        CSeq_feat_EditHandle feat_eh(*feat_ci);
                        feat_eh.Replace(*copy_feat);
                    }
                }

                // Create a copy of inst
                CRef<CSeq_inst> copy_inst(new CSeq_inst());
                copy_inst->Assign(bsh.GetInst());

                // Make changes to the inst copy 
                BOOST_CHECK_NO_THROW(edit::TrimSeqData( bsh, copy_inst, sorted_cuts ));

                // Update the input seqentry with the changes
                bsh.GetEditHandle().SetInst(*copy_inst);
            }
        }

        // Are the changes what we expect?
        BOOST_CHECK( s_AreSeqEntriesEqualAndPrintIfNot(
             *entry_h.GetCompleteSeq_entry(),
             *expected_entry_h.GetCompleteSeq_entry()) );

        BOOST_CHECK_NO_THROW( s_pScope->RemoveTopLevelSeqEntry(entry_h) );
        BOOST_CHECK_NO_THROW( s_pScope->RemoveTopLevelSeqEntry(expected_entry_h) );
    }
}


BOOST_AUTO_TEST_CASE(TrimSeqFeat_Featured_Trimmed)
{
    cout << "Testing FUNCTION: TrimSeqFeat - cdregion feature was trimmed" << endl;

    TMapTestNameToTestFiles & mapOfTests = s_mapFunctionToVecOfTests["trim_seq_feat_feature_trimmed"];

    BOOST_CHECK( ! mapOfTests.empty() );

    NON_CONST_ITERATE( TMapTestNameToTestFiles, test_it, mapOfTests ) {
        const string & sTestName = (test_it->first);
        cout << "Running TEST: " << sTestName << endl;

        TMapTestFiles & test_stage_map = (test_it->second);

        BOOST_REQUIRE( test_stage_map.size() == 2u );

        // Get the input/output files
        const CFile & input_entry_file = test_stage_map["input_entry"];
        const CFile & output_expected_file = test_stage_map["output_expected"];

        CRef<CSeq_entry> pInputEntry = s_ReadAndPreprocessEntry( input_entry_file.GetPath() );
        CRef<CSeq_entry> pOutputExpectedEntry = s_ReadAndPreprocessEntry( output_expected_file.GetPath() );

        CSeq_entry_Handle entry_h = s_pScope->AddTopLevelSeqEntry(*pInputEntry);
        CSeq_entry_Handle expected_entry_h = s_pScope->AddTopLevelSeqEntry(*pOutputExpectedEntry);

        // Find the bioseq(s) that we will trim
        CBioseq_CI bioseq_ci( entry_h );
        for( ; bioseq_ci; ++bioseq_ci ) {
            const CBioseq_Handle& bsh = *bioseq_ci;

            if (s_FindLocalId(bsh, "BankIt1717834")) {
                // Create the cuts from known vector contamination
                // Seqid "BankIt1717834" has vector
                edit::TRange cut1(0, 653);
                edit::TCuts cuts;
                cuts.push_back(cut1);

                // Sort the cuts
                edit::TCuts sorted_cuts;
                BOOST_CHECK_NO_THROW(edit::GetSortedCuts( bsh, cuts, sorted_cuts ));

                // Create a copy of inst
                CRef<CSeq_inst> copy_inst(new CSeq_inst());
                copy_inst->Assign(bsh.GetInst());

                // Make changes to the inst copy 
                BOOST_CHECK_NO_THROW(edit::TrimSeqData( bsh, copy_inst, sorted_cuts ));

                // Iterate over bioseq features
                SAnnotSelector feat_sel(CSeq_annot::C_Data::e_Ftable);
                CFeat_CI feat_ci(bsh, feat_sel);
                for (; feat_ci; ++feat_ci) {
                    // Make a copy of the feature
                    CRef<CSeq_feat> copy_feat(new CSeq_feat());
                    copy_feat->Assign(feat_ci->GetOriginalFeature());

                    // Detect complete deletions of feature
                    bool bFeatureDeleted = false;

                    // Detect case where feature was not deleted but merely trimmed
                    bool bFeatureTrimmed = false;

                    // Modify the copy of the feature
                    BOOST_CHECK_NO_THROW(edit::TrimSeqFeat(copy_feat, sorted_cuts, bFeatureDeleted, bFeatureTrimmed));

                    if (bFeatureDeleted) {
                        // Not testing this branch in this unit test.

                        // Delete the feature
                        // If the feature was a cdregion, delete the protein and
                        // renormalize the nuc-prot set
                        //DeleteProteinAndRenormalizeNucProtSet(*feat_ci);
                    }
                    else 
                    if (bFeatureTrimmed) {
                        // Further modify the copy of the feature

                        // If this feat is a Cdregion, then RETRANSLATE the protein
                        // sequence AND adjust any protein feature
                        if ( copy_feat->IsSetData() && 
                             copy_feat->GetData().Which() == CSeqFeatData::e_Cdregion &&
                             copy_feat->IsSetProduct() )
                        {
                            // Get length of nuc sequence before trimming
                            TSeqPos original_nuc_len = 0;
                            if (bsh.CanGetInst() && bsh.GetInst().CanGetLength()) {
                                original_nuc_len = bsh.GetInst().GetLength();
                            }
                            BOOST_CHECK_NO_THROW(edit::AdjustCdregionFrame(original_nuc_len, copy_feat, sorted_cuts));

                            // Retranslate the coding region using the new nuc sequence
                            BOOST_CHECK_NO_THROW(edit::RetranslateCdregion(bsh, copy_inst, copy_feat, sorted_cuts));
                        }

                        // Update the original feature with the modified copy
                        CSeq_feat_EditHandle feat_eh(*feat_ci);
                        feat_eh.Replace(*copy_feat);
                    }
                }

                // Update the input seqentry with the changes
                bsh.GetEditHandle().SetInst(*copy_inst);
            }
        }

        // Are the changes what we expect?
        BOOST_CHECK( s_AreSeqEntriesEqualAndPrintIfNot(
             *entry_h.GetCompleteSeq_entry(),
             *expected_entry_h.GetCompleteSeq_entry()) );

        BOOST_CHECK_NO_THROW( s_pScope->RemoveTopLevelSeqEntry(entry_h) );
        BOOST_CHECK_NO_THROW( s_pScope->RemoveTopLevelSeqEntry(expected_entry_h) );
    }
}


BOOST_AUTO_TEST_CASE(TrimSequenceAndAnnotation)
{
    cout << "Testing FUNCTION: TrimSequenceAndAnnotation" << endl;

    TMapTestNameToTestFiles & mapOfTests = s_mapFunctionToVecOfTests["trim_sequence_and_annotation"];

    BOOST_CHECK( ! mapOfTests.empty() );

    NON_CONST_ITERATE( TMapTestNameToTestFiles, test_it, mapOfTests ) {
        const string & sTestName = (test_it->first);
        cout << "Running TEST: " << sTestName << endl;

        TMapTestFiles & test_stage_map = (test_it->second);

        BOOST_REQUIRE( test_stage_map.size() == 2u );

        // Get the input/output files
        const CFile & input_entry_file = test_stage_map["input_entry"];
        const CFile & output_expected_file = test_stage_map["output_expected"];

        CRef<CSeq_entry> pInputEntry = s_ReadAndPreprocessEntry( input_entry_file.GetPath() );
        CRef<CSeq_entry> pOutputExpectedEntry = s_ReadAndPreprocessEntry( output_expected_file.GetPath() );

        CSeq_entry_Handle entry_h = s_pScope->AddTopLevelSeqEntry(*pInputEntry);
        CSeq_entry_Handle expected_entry_h = s_pScope->AddTopLevelSeqEntry(*pOutputExpectedEntry);

        // Find the bioseq(s) that we will trim
        CBioseq_CI bioseq_ci( entry_h );
        for( ; bioseq_ci; ++bioseq_ci ) {
            const CBioseq_Handle& bsh = *bioseq_ci;

            // Seq4 is found in test1 input
            if (s_FindLocalId(bsh, "Seq4")) {
                // Create the cuts from known vector contamination
                // Seqid "Seq4" has vector
                edit::TRange cut1(376, 596);
                edit::TRange cut2(0, 92);
                edit::TRange cut3(93, 108);
                edit::TRange cut4(109, 188);
                edit::TRange cut5(662, 671);
                edit::TRange cut6(672, 690);
                edit::TCuts cuts;
                cuts.push_back(cut1);
                cuts.push_back(cut2);
                cuts.push_back(cut3);
                cuts.push_back(cut4);
                cuts.push_back(cut5);
                cuts.push_back(cut6);

                BOOST_CHECK_NO_THROW(edit::TrimSequenceAndAnnotation( bsh, cuts, edit::eTrimToClosestEnd ));
            }

            // trpB is found in test2 input
            if (s_FindLocalId(bsh, "trpB")) {
                // Create the cuts from known vector contamination
                // Seqid "trpB" has vector
                edit::TRange cut1(0, 2);
                edit::TRange cut2(3, 68);
                edit::TRange cut3(69, 86);
                edit::TRange cut4(87, 119);
                edit::TCuts cuts;
                cuts.push_back(cut1);
                cuts.push_back(cut2);
                cuts.push_back(cut3);
                cuts.push_back(cut4);

                BOOST_CHECK_NO_THROW(edit::TrimSequenceAndAnnotation( bsh, cuts, edit::eTrimToClosestEnd ));
            }

            // Seq1 is found in test3 input
            if (s_FindLocalId(bsh, "Seq1")) {
                // Create the cuts from known vector contamination
                // Seqid "Seq1" has vector
                edit::TRange cut1(0, 141);
                edit::TRange cut2(2080, 3035);
                edit::TRange cut3(285, 325);
                edit::TRange cut4(326, 359);
                edit::TRange cut5(360, 403);
                edit::TCuts cuts;
                cuts.push_back(cut1);
                cuts.push_back(cut2);
                cuts.push_back(cut3);
                cuts.push_back(cut4);
                cuts.push_back(cut5);

                BOOST_CHECK_NO_THROW(edit::TrimSequenceAndAnnotation( bsh, cuts, edit::eTrimToClosestEnd ));
            }
        }

        // Are the changes what we expect?
        BOOST_CHECK( s_AreSeqEntriesEqualAndPrintIfNot(
             *entry_h.GetCompleteSeq_entry(),
             *expected_entry_h.GetCompleteSeq_entry()) );

        BOOST_CHECK_NO_THROW( s_pScope->RemoveTopLevelSeqEntry(entry_h) );
        BOOST_CHECK_NO_THROW( s_pScope->RemoveTopLevelSeqEntry(expected_entry_h) );
    }
}


BOOST_AUTO_TEST_CASE(TrimSequenceAndAnnotation_InvalidInput)
{
    cout << "Testing FUNCTION: TrimSequenceAndAnnotation with invalid input" << endl;

    TMapTestNameToTestFiles & mapOfTests = s_mapFunctionToVecOfTests["trim_sequence_and_annotation_invalid_input"];

    BOOST_CHECK( ! mapOfTests.empty() );

    NON_CONST_ITERATE( TMapTestNameToTestFiles, test_it, mapOfTests ) {
        const string & sTestName = (test_it->first);
        cout << "Running TEST: " << sTestName << endl;

        TMapTestFiles & test_stage_map = (test_it->second);

        BOOST_REQUIRE( test_stage_map.size() == 1u );

        // Need input file only
        const CFile & input_entry_file = test_stage_map["input_entry"];

        CRef<CSeq_entry> pInputEntry = s_ReadAndPreprocessEntry( input_entry_file.GetPath() );

        CSeq_entry_Handle entry_h = s_pScope->AddTopLevelSeqEntry(*pInputEntry);

        // Find the bioseq(s) of interest that we will check
        CBioseq_CI bioseq_ci( entry_h );
        for( ; bioseq_ci; ++bioseq_ci ) {
            const CBioseq_Handle& bsh = *bioseq_ci;

            // Deliberately use invalid cut locations for Seq4 which has length 691
            if (s_FindLocalId(bsh, "Seq4")) {
                // Invalid "from" value of -1
                edit::TCuts cuts;
                cuts.push_back(edit::TRange(-1, 100));
                BOOST_CHECK_THROW(edit::TrimSequenceAndAnnotation( bsh, cuts, edit::eTrimToClosestEnd ), edit::CEditException);

                // Invalid "to" value of 691
                cuts.clear();
                cuts.push_back(edit::TRange(100, 691));
                BOOST_CHECK_THROW(edit::TrimSequenceAndAnnotation( bsh, cuts, edit::eTrimToClosestEnd ), edit::CEditException);
            }

            // Deliberately specify cuts for a protein sequence!
            if (s_FindLocalId(bsh, "Seq4_prot_4")) {
                // Not a nuc bioseq!
                edit::TCuts cuts;
                cuts.push_back(edit::TRange(0, 10));
                BOOST_CHECK_THROW(edit::TrimSequenceAndAnnotation( bsh, cuts, edit::eTrimToClosestEnd ), edit::CEditException);
            }
        }

        BOOST_CHECK_NO_THROW( s_pScope->RemoveTopLevelSeqEntry(entry_h) );
    }
}


BOOST_AUTO_TEST_CASE(Test_Unverified)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();

    BOOST_CHECK_EQUAL(edit::IsUnverifiedOrganism(entry->GetSeq()), false);
    BOOST_CHECK_EQUAL(edit::IsUnverifiedFeature(entry->GetSeq()), false);

    CRef<CSeqdesc> new_unv(new CSeqdesc());
    new_unv->SetUser();
    entry->SetSeq().SetDescr().Set().push_back(new_unv);
    new_unv->SetUser().AddUnverifiedOrganism();
    BOOST_CHECK_EQUAL(edit::IsUnverifiedOrganism(entry->GetSeq()), true);
    BOOST_CHECK_EQUAL(edit::IsUnverifiedFeature(entry->GetSeq()), false);
    BOOST_CHECK_EQUAL(edit::IsUnverifiedMisassembled(entry->GetSeq()), false);
    CRef<CSeqdesc> unv = edit::FindUnverified(entry->GetSeq());
    BOOST_CHECK_EQUAL(unv.GetPointer(), new_unv.GetPointer());

    new_unv->SetUser().AddUnverifiedFeature();
    BOOST_CHECK_EQUAL(edit::IsUnverifiedOrganism(entry->GetSeq()), true);
    BOOST_CHECK_EQUAL(edit::IsUnverifiedFeature(entry->GetSeq()), true);
    BOOST_CHECK_EQUAL(edit::IsUnverifiedMisassembled(entry->GetSeq()), false);
    unv = edit::FindUnverified(entry->GetSeq());
    BOOST_CHECK_EQUAL(unv.GetPointer(), new_unv.GetPointer());

    new_unv->SetUser().RemoveUnverifiedOrganism();
    BOOST_CHECK_EQUAL(edit::IsUnverifiedOrganism(entry->GetSeq()), false);
    BOOST_CHECK_EQUAL(edit::IsUnverifiedFeature(entry->GetSeq()), true);
    BOOST_CHECK_EQUAL(edit::IsUnverifiedMisassembled(entry->GetSeq()), false);
    unv = edit::FindUnverified(entry->GetSeq());
    BOOST_CHECK_EQUAL(unv.GetPointer(), new_unv.GetPointer());

    new_unv->SetUser().RemoveUnverifiedFeature();
    BOOST_CHECK_EQUAL(edit::IsUnverifiedOrganism(entry->GetSeq()), false);
    BOOST_CHECK_EQUAL(edit::IsUnverifiedFeature(entry->GetSeq()), false);
    BOOST_CHECK_EQUAL(edit::IsUnverifiedMisassembled(entry->GetSeq()), false);
    unv = edit::FindUnverified(entry->GetSeq());
    BOOST_CHECK_EQUAL(unv.GetPointer(), new_unv.GetPointer());
    
    new_unv->SetUser().AddUnverifiedMisassembled();
    BOOST_CHECK_EQUAL(edit::IsUnverifiedOrganism(entry->GetSeq()), false);
    BOOST_CHECK_EQUAL(edit::IsUnverifiedFeature(entry->GetSeq()), false);
    BOOST_CHECK_EQUAL(edit::IsUnverifiedMisassembled(entry->GetSeq()), true);
    unv = edit::FindUnverified(entry->GetSeq());
    BOOST_CHECK_EQUAL(unv.GetPointer(), new_unv.GetPointer());


}


BOOST_AUTO_TEST_CASE(Test_SeqEntryFromSeqSubmit)
{
    CRef<CSeq_submit> submit(new CSeq_submit());
    CRef<CSeq_entry> wrapped = unit_test_util::BuildGoodSeq();
    submit->SetData().SetEntrys().push_back(wrapped);
    submit->SetSub().SetCit().SetAuthors().SetNames().SetStd().push_back(unit_test_util::BuildGoodAuthor());

    CRef<CSeq_entry> entry = edit::SeqEntryFromSeqSubmit(*submit);
    BOOST_CHECK_EQUAL(entry->IsSeq(), true);
    BOOST_CHECK_EQUAL(entry->GetSeq().GetDescr().Get().size(), wrapped->GetSeq().GetDescr().Get().size() + 1);

    // try again with cit-sub pub already there
    submit->SetData().SetEntrys().front()->Assign(*entry);
    entry = edit::SeqEntryFromSeqSubmit(*submit);
    BOOST_CHECK_EQUAL(entry->GetSeq().GetDescr().Get().size(), submit->GetData().GetEntrys().front()->GetSeq().GetDescr().Get().size());

    wrapped = unit_test_util::BuildGoodNucProtSet();
    submit->SetData().SetEntrys().front()->Assign(*wrapped);

    entry = edit::SeqEntryFromSeqSubmit(*submit);
    BOOST_CHECK_EQUAL(entry->IsSet(), true);
    BOOST_CHECK_EQUAL(entry->GetSet().GetDescr().Get().size(), wrapped->GetSet().GetDescr().Get().size() + 1);
    // try again with cit-sub pub already there
    submit->SetData().SetEntrys().front()->Assign(*entry);
    entry = edit::SeqEntryFromSeqSubmit(*submit);
    BOOST_CHECK_EQUAL(entry->GetSet().GetDescr().Get().size(), submit->GetData().GetEntrys().front()->GetSet().GetDescr().Get().size());

}


BOOST_AUTO_TEST_CASE(Test_GetTargetedLocusNameConsensus)
{
    BOOST_CHECK_EQUAL(edit::GetTargetedLocusNameConsensus(kEmptyStr, "16S ribosomal RNA"), "16S ribosomal RNA");
    BOOST_CHECK_EQUAL(edit::GetTargetedLocusNameConsensus("16S ribosomal RNA", kEmptyStr), "16S ribosomal RNA");
    BOOST_CHECK_EQUAL(edit::GetTargetedLocusNameConsensus("16S ribosomal RNA conserved region", "16S ribosomal RNA"), "16S ribosomal RNA");
    BOOST_CHECK_EQUAL(edit::GetTargetedLocusNameConsensus("16S ribosomal RNA", "16S ribosomal RNA conserved region"), "16S ribosomal RNA");
    BOOST_CHECK_EQUAL(edit::GetTargetedLocusNameConsensus("conserved region 16S ribosomal RNA", "16S ribosomal RNA"), "16S ribosomal RNA");
    BOOST_CHECK_EQUAL(edit::GetTargetedLocusNameConsensus("16S ribosomal RNA", "conserved region 16S ribosomal RNA"), "16S ribosomal RNA");
    BOOST_CHECK_EQUAL(edit::GetTargetedLocusNameConsensus("abc 16S ribosomal RNA 456", "16S ribosomal RNA"), "16S ribosomal RNA");
    BOOST_CHECK_EQUAL(edit::GetTargetedLocusNameConsensus("16S ribosomal RNA", "abc 16S ribosomal RNA 456"), "16S ribosomal RNA");
    BOOST_CHECK_EQUAL(edit::GetTargetedLocusNameConsensus("something 16S ribosomal RNA else", "abc 16S ribosomal RNA 456"), "16S ribosomal RNA");
    BOOST_CHECK_EQUAL(edit::GetTargetedLocusNameConsensus("this is not a match", "something else entirely"), kEmptyStr);
}


void CheckTargetedLocusEntry(CRef<CSeq_entry> entry, const string& expected)
{
    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*object_manager));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    CBioseq_CI bi(seh, CSeq_inst::eMol_na);
    BOOST_CHECK_EQUAL(edit::GenerateTargetedLocusName(*bi), expected);
}


BOOST_AUTO_TEST_CASE(Test_GetTargetedLocusName)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_feat> prot = unit_test_util::GetProtFeatFromGoodNucProtSet(entry);
    BOOST_CHECK_EQUAL(edit::GetTargetedLocusName(*prot), "fake protein name");
    CheckTargetedLocusEntry(entry, "fake protein name");

    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    CRef<CSeq_feat> gene = unit_test_util::MakeGeneForFeature(cds);
    gene->SetData().SetGene().SetLocus("XYZ");
    BOOST_CHECK_EQUAL(edit::GetTargetedLocusName(*gene), "XYZ");
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    unit_test_util::AddFeat(gene, nuc);
    CheckTargetedLocusEntry(entry, "XYZ");
    
    CRef<CSeq_feat> rna = unit_test_util::AddMiscFeature(entry);    
    rna->SetData().SetRna().SetType(CRNA_ref::eType_rRNA);
    string remainder;
    rna->SetData().SetRna().SetRnaProductName("18S ribosomal RNA", remainder);
    BOOST_CHECK_EQUAL(edit::GetTargetedLocusName(*rna), "18S ribosomal RNA");

    CRef<CSeq_feat> imp = unit_test_util::AddMiscFeature(entry);
    imp->SetData().SetImp().SetKey("misc_feature");
    imp->SetComment("uce");
    BOOST_CHECK_EQUAL(edit::GetTargetedLocusName(*imp), "uce");
}


END_SCOPE(objects)
END_NCBI_SCOPE
