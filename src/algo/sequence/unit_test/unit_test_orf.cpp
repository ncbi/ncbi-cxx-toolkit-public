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


#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/feat_ci.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/Org_ref.hpp>

#include <algo/sequence/orf.hpp>
#include <objmgr/util/sequence.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


NCBITEST_AUTO_INIT()
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
}


NCBITEST_INIT_CMDLINE(arg_desc)
{
    arg_desc->AddKey("in", "input",
                     "Seq-annot of ORFs",
                     CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey("out", "output",
                     "Output seq-annot of ORFs",
                     CArgDescriptions::eOutputFile);
}

BOOST_AUTO_TEST_CASE(TestUsingArg)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CScope scope(*om);
    scope.AddDefaults();

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    CNcbiIstream& istr = args["in"].AsInputFile();
    CNcbiOstream* ostr = args["out"] ? &args["out"].AsOutputFile() : NULL;

    while(true) {

        CRef<CSeq_annot> ref_annot(new CSeq_annot);

        try {
            istr >> MSerial_AsnText >> *ref_annot;
        } catch (CEofException&) {
            break;
        }

        const CAnnot_descr& desc = ref_annot->GetDesc();
        string comment = desc.Get().front()->GetComment();

        vector<string> allowable_starts;
        bool longest_orfs = false;
        NStr::Split(comment, " ", allowable_starts, NStr::fSplit_Tokenize);
        auto longest = find(allowable_starts.begin(), allowable_starts.end(), "longest");
        if (longest != allowable_starts.end()) {
            longest_orfs = true;
            allowable_starts.erase(longest);
        }

        const CSeq_id& seq_id = *ref_annot->GetData().GetFtable().front()->GetLocation().GetId();
        CBioseq_Handle bsh = scope.GetBioseqHandle(seq_id);

        const COrg_ref &org = sequence::GetOrg_ref(bsh);
        int gcode = org.IsSetGcode() ? org.GetGcode() : 1;


        COrf::TLocVec loc_vec;
        {{
            CSeqVector sv(bsh);
            size_t min_length = 300; //=100 codons options in gbench
            size_t max_seq_gap = 30;
            COrf::FindOrfs(sv, loc_vec, min_length, gcode, allowable_starts, longest_orfs, max_seq_gap);
        }}

        CRef<CSeq_annot> my_annot = COrf::MakeCDSAnnot(loc_vec, gcode, const_cast<CSeq_id*>(&seq_id));

        my_annot->SetDesc().Assign(desc);

        if(ostr) {
            *ostr << MSerial_AsnText << *my_annot;
        }

        BOOST_CHECK(my_annot->Equals(*ref_annot));
    }
}


BOOST_AUTO_TEST_CASE(tiny_islands)
{
    string seq =
        "ANNNNNNNNNNNNANNNNNNNNNNNNAANNNNNNNNNNNNAA"
        "ANNNNNNNNNNNNATGTGANNNNNNNNNNNNATGTGAANNNNNNNNNNNNAA";
    vector<string> allowable_starts = {{"ATG"}};
    COrf::TLocVec loc_vec;
    BOOST_CHECK_NO_THROW(
        COrf::FindOrfs(seq, loc_vec, 0, 1, allowable_starts, false, 10)
        );
    BOOST_CHECK_EQUAL(loc_vec.size(), 2);
}

BOOST_AUTO_TEST_CASE(first_n)
{
    // the very last N caused lookup beyond sequence
    // the very first N would become the last on minus strand,
    // can be extended beyond sequence and
    // become negative when converted back
    string seq =
        "NTCACCTTTTCGCCCCTCGGCGACTTACTTTGAGAGGCCAAAGTAAGCAAAGCCTTTTGCTCCGGTTCC";
    vector<string> allowable_starts;
    COrf::TLocVec loc_vec;
    BOOST_CHECK_NO_THROW(COrf::FindOrfs(seq, loc_vec, 60, 11, allowable_starts,
                                        true,  // longest_orfs
                                        10000));
    for (auto& loc: loc_vec) {
        BOOST_CHECK(loc->GetStart(eExtreme_Positional) <
                    loc->GetStop(eExtreme_Positional));
    }
    BOOST_CHECK(loc_vec.size() > 0);
}
BOOST_AUTO_TEST_CASE(short_n)
{
    // the first N used to be ignored in codon state calculation
    string seq =
        "ATGCTGANAA"
        "ATGTTGNAAA"
        "ATGCCCTGA";
    vector<string> allowable_starts = {"ATG"};
    COrf::TLocVec loc_vec;
    BOOST_CHECK_NO_THROW(COrf::FindOrfs(seq, loc_vec, 3, 11, allowable_starts,
                                        true,  // longest_orfs
                                        10000));
    for (auto& loc: loc_vec) {
        cerr << loc->GetStart(eExtreme_Biological) << ".." <<
            loc->GetStop(eExtreme_Biological) << endl;
    }
    BOOST_CHECK_EQUAL(count_if(loc_vec.begin(),loc_vec.end(),
                            [](const CRef<CSeq_loc>& a){
                                return !a->IsPartialStop(eExtreme_Biological);
                            }),
                      1);
}
