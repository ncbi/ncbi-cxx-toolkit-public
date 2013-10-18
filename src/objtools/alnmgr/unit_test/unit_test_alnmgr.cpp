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
* Author:  Aleksey Grichenko
*
* File Description:
*   Alignment Manager unit test.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/test_boost.hpp>

#include <serial/objistr.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Object_id.hpp>

#include <objtools/alnmgr/aln_user_options.hpp>
#include <objtools/alnmgr/aln_asn_reader.hpp>
#include <objtools/alnmgr/aln_container.hpp>
#include <objtools/alnmgr/aln_tests.hpp>
#include <objtools/alnmgr/aln_stats.hpp>
#include <objtools/alnmgr/pairwise_aln.hpp>
#include <objtools/alnmgr/aln_serial.hpp>
#include <objtools/alnmgr/sparse_aln.hpp>
#include <objtools/alnmgr/aln_converters.hpp>
#include <objtools/alnmgr/aln_builders.hpp>
#include <objtools/alnmgr/sparse_ci.hpp>
#include <objtools/alnmgr/aln_generators.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);



NCBITEST_INIT_CMDLINE(descrs)
{
    descrs->AddFlag("print_exp", "Print expected values instead of testing");
}


bool DumpExpected(void)
{
    return CNcbiApplication::Instance()->GetArgs()["print_exp"];
}


CScope& GetScope(void)
{
    static CRef<CScope> s_Scope;
    if ( !s_Scope ) {
        CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
        CGBDataLoader::RegisterInObjectManager(*objmgr);
        
        s_Scope.Reset(new CScope(*objmgr));
        s_Scope->AddDefaults();
    }
    return *s_Scope;
}


// Helper function for loading alignments. Returns number of alignments loaded
// into the container. The expected format is:
// <int-number-of-alignments>
// <Seq-align>+
size_t LoadAligns(CNcbiIstream& in,
                  CAlnContainer& aligns,
                  size_t limit = 0)
{
    size_t num_aligns = 0;
    while ( !in.eof() ) {
        try {
            CRef<CSeq_align> align(new CSeq_align);
            in >> MSerial_AsnText >> *align;
            align->Validate(true);
            aligns.insert(*align);
            num_aligns++;
            if (limit > 0  &&  num_aligns >= limit) break;
        }
        catch (CIOException e) {
            break;
        }
    }
    return num_aligns;
}


// The aln_id_map argument is required because TScopeAlnStat keeps const
// reference to the id-map. So, the map needs to be returned to the caller.
CRef<TScopeAlnStats> InitAlnStats(const CAlnContainer& aligns,
                                  auto_ptr<TScopeAlnIdMap>& aln_id_map)
{
    TScopeAlnSeqIdConverter id_conv(&GetScope());
    TScopeIdExtract id_extract(id_conv);
    aln_id_map.reset(new TScopeAlnIdMap(id_extract, aligns.size()));
    ITERATE(CAlnContainer, aln_it, aligns) {
        aln_id_map->push_back(**aln_it);
    }
    CRef<TScopeAlnStats> aln_stats(new TScopeAlnStats(*aln_id_map));
    return aln_stats;
}


void CheckPairwiseAln(CNcbiIstream&       in_exp,
                      const CPairwiseAln& pw,
                      size_t aln_idx,
                      int anchor,
                      int row)
{
    // For each alignment/anchor/row:
    // <int-align-index> <int-anchor_index> <int-row-index> <int-pairwise-flags>
    //   <int-align-first-from> <int-align-first-to> <int-base-width> <Seq-id>
    //   <int-align-second-from> <int-align-second-to> <int-base-width> <Seq-id>
    //     For each CPairwiseAln element:
    //     <int-first-from> <int-second-from> <int-len> <int-is-direct>
    //   <int-insertions-count>
    //     For each insertion:
    //     <int-first-from> <int-second-from> <int-len> <int-is-direct>
    //   For each iterator:
    //   <int-first-from> <int-first-to> <int-second-from> <int-second-to> ...
    //   ... <int-direct> <int-first-direct> <int-aligned>

    TSignedSeqPos first_from, first_to, second_from, second_to, len;
    int flags;

    if ( DumpExpected() ) {
        cout << aln_idx << " " << anchor << " " << row << " " << pw.GetFlags() << endl;
        cout << "  " << pw.GetFirstFrom() << " "
            << pw.GetFirstTo() << " "
            << pw.GetFirstBaseWidth() << " "
            << MSerial_AsnText << pw.GetFirstId()->GetSeqId();
        cout << "  " << pw.GetSecondPosByFirstPos(pw.GetFirstFrom()) << " "
            << pw.GetSecondPosByFirstPos(pw.GetFirstTo()) << " "
            << pw.GetSecondBaseWidth() << " "
            << MSerial_AsnText << pw.GetSecondId()->GetSeqId();
    }
    else {
        size_t expected_aln_idx;
        int expected_row, expected_anchor;
        int first_width, second_width;
        CSeq_id first_id, second_id;
        in_exp >> expected_aln_idx >> expected_anchor >> expected_row >> flags;
        in_exp >> first_from >> first_to >> first_width;
        in_exp >> MSerial_AsnText >> first_id;
        in_exp >> second_from >> second_to >> second_width;
        in_exp >> MSerial_AsnText >> second_id;
        BOOST_CHECK(in_exp.good());
        BOOST_CHECK(aln_idx == expected_aln_idx);
        BOOST_CHECK(anchor == expected_anchor);
        BOOST_CHECK(row == expected_row);
        BOOST_CHECK(pw.GetFlags() == flags);
        BOOST_CHECK(pw.GetFirstId()->GetSeqId().Equals(first_id));
        BOOST_CHECK(pw.GetSecondId()->GetSeqId().Equals(second_id));
        BOOST_CHECK(pw.GetFirstBaseWidth() == first_width);
        BOOST_CHECK(pw.GetSecondBaseWidth() == second_width);
        BOOST_CHECK(pw.GetFirstFrom() == first_from);
        BOOST_CHECK(pw.GetFirstTo() == first_to);
        BOOST_CHECK(pw.GetSecondPosByFirstPos(pw.GetFirstFrom()) == second_from);
        BOOST_CHECK(pw.GetSecondPosByFirstPos(pw.GetFirstTo()) == second_to);
    }

    ITERATE(CPairwiseAln, rg_it, pw) {
        CAlignRange<TSignedSeqPos> rg = *rg_it;

        if ( DumpExpected() ) {
            cout << "    " << rg_it->GetFirstFrom() << " "
                << rg_it->GetSecondFrom() << " "
                << rg_it->GetLength() << " "
                << (rg.IsDirect() ? 1 : 0) << endl;
        }
        else {
            in_exp >> first_from >> second_from >> len >> flags;
            BOOST_CHECK(in_exp.good());
            BOOST_CHECK(rg_it->GetFirstFrom() == first_from);
            BOOST_CHECK(rg_it->GetSecondFrom() == second_from);
            BOOST_CHECK(rg_it->GetLength() == len);
            BOOST_CHECK(rg_it->IsDirect() == (flags != 0));
        }
    }

    if ( DumpExpected() ) {
        cout << "  " << pw.GetInsertions().size() << endl;
    }
    else {
        size_t ins_count;
        in_exp >> ins_count;
        BOOST_CHECK(in_exp.good());
        BOOST_CHECK(pw.GetInsertions().size() == ins_count);
    }

    if (!pw.GetInsertions().empty()) {
        ITERATE(CPairwiseAln::TAlignRangeVector, gap, pw.GetInsertions()) {
            if ( DumpExpected() ) {
                cout << "    " << gap->GetFirstFrom() << " "
                    << gap->GetSecondFrom() << " "
                    << gap->GetLength() << " "
                    << (gap->IsDirect() ? 1 : 0) << endl;
            }
            else {
                in_exp >> first_from >> second_from >> len >> flags;
                BOOST_CHECK(in_exp.good());
                BOOST_CHECK(gap->GetFirstFrom() == first_from);
                BOOST_CHECK(gap->GetSecondFrom() == second_from);
                BOOST_CHECK(gap->GetLength() == len);
                BOOST_CHECK(gap->IsDirect() == (flags != 0));
            }
        }
    }
}


void CheckSparseAln(CNcbiIstream&     in_exp,
                    const CSparseAln& sparse,
                    bool              check_sequence)
{
    // Format:
    // <dim> <numrows> <anchor row>
    // <aln-from> <aln-to>
    // For each row:
    // <row> <width> <seq-id>
    // <aln-from> <aln_to> <seq-from> <seq-to> <native-from> <native-to>
    // [<aln-sequence>]
    // [<row-sequence>]

    static const char* kGap = "<GAP>";
    static const char* kNoData = "<NO SEQUENCE DATA>";

    TSignedSeqPos expected_aln_from, expected_aln_to;

    if ( DumpExpected() ) {
        cout << sparse.GetDim() << " "
            << sparse.GetNumRows() << " "
            << sparse.GetAnchor() << endl;
        cout << sparse.GetAlnRange().GetFrom() << " "
            << sparse.GetAlnRange().GetTo() << endl;
    }
    else {
        int expected_dim, expected_numrows, expected_anchor;
        in_exp >> expected_dim >> expected_numrows >> expected_anchor;
        in_exp >> expected_aln_from >> expected_aln_to;
        BOOST_CHECK(in_exp.good());
        BOOST_CHECK(sparse.GetDim() == expected_dim);
        BOOST_CHECK(sparse.GetNumRows() == expected_numrows);
        BOOST_CHECK(sparse.GetAnchor() == expected_anchor);
        BOOST_CHECK(sparse.GetAlnRange().GetFrom() == expected_aln_from);
        BOOST_CHECK(sparse.GetAlnRange().GetTo() == expected_aln_to);
    }

    for (CSparseAln::TDim row = 0; row < sparse.GetDim(); ++row) {
        if ( DumpExpected() ) {
            cout << row << " "
                << sparse.GetBaseWidth(row) << " "
                << MSerial_AsnText << sparse.GetSeqId(row);
        }
        else {
            int expected_row, expected_width;
            CSeq_id expected_id;
            in_exp >> expected_row >> expected_width >> MSerial_AsnText >> expected_id;
            BOOST_CHECK(in_exp.good());
            BOOST_CHECK(row == expected_row);
            BOOST_CHECK(sparse.GetBaseWidth(row) == expected_width);
            BOOST_CHECK(sparse.GetSeqId(row).Equals(expected_id));
        }

        CSparseAln::TRange rg = sparse.GetSeqRange(row);
        CSparseAln::TRange native_rg = sparse.AlnRangeToNativeSeqRange(row, rg);

        TSeqPos expected_seq_from, expected_seq_to,
            expected_native_from, expected_native_to;

        if ( DumpExpected() ) {
            cout << sparse.GetSeqAlnStart(row) << " " << sparse.GetSeqAlnStop(row) << " "
                << rg.GetFrom() << " " << rg.GetTo() << " "
                << native_rg.GetFrom() << " " << native_rg.GetTo() << endl;
        }
        else {
            in_exp >> expected_aln_from >> expected_aln_to
                >> expected_seq_from >> expected_seq_to
                >> expected_native_from >> expected_native_to;
            BOOST_CHECK(in_exp.good());
            BOOST_CHECK(sparse.GetSeqAlnStart(row) == expected_aln_from);
            BOOST_CHECK(sparse.GetSeqAlnStop(row) == expected_aln_to);
            BOOST_CHECK(rg.GetFrom() == expected_seq_from);
            BOOST_CHECK(rg.GetTo() == expected_seq_to);
            BOOST_CHECK(native_rg.GetFrom() == expected_native_from);
            BOOST_CHECK(native_rg.GetTo() == expected_native_to);
        }

        if ( check_sequence ) {
            string aln_sequence, row_sequence;
            sparse.GetAlnSeqString(row, aln_sequence, CSparseAln::TSignedRange::GetWhole());
            sparse.GetSeqString(row, row_sequence, CSparseAln::TRange::GetWhole());
            if ( aln_sequence.empty() ) {
                aln_sequence = kNoData;
            }
            if ( row_sequence.empty() ) {
                row_sequence = kNoData;
            }

            if ( DumpExpected() ) {
                cout << aln_sequence << endl;
                cout << row_sequence << endl;
            }
            else {
                string expected_aln_sequence, expected_row_sequence;
                in_exp >> ws;
                getline(in_exp, expected_aln_sequence);
                getline(in_exp, expected_row_sequence);
                BOOST_CHECK(in_exp.good());
                BOOST_CHECK(aln_sequence == expected_aln_sequence);
                BOOST_CHECK(row_sequence == expected_row_sequence);
            }
        }

        CSparse_CI sparse_ci(sparse, row, CSparse_CI::eAllSegments);
        for (; sparse_ci; ++sparse_ci) {
            const IAlnSegment& seg = *sparse_ci;
            CSparse_CI::TSignedRange aln_rg = seg.GetAlnRange();
            CSparse_CI::TSignedRange seq_rg = seg.GetRange();

            if ( DumpExpected() ) {
                cout << "  " << seg.GetType() << " "
                    << aln_rg.GetFrom() << " " << aln_rg.GetTo() << " "
                    << seq_rg.GetFrom() << " " << seq_rg.GetTo() << endl;
            }
            else {
                unsigned expected_seg_type;
                TSignedSeqPos expected_seq_from, expected_seq_to;

                in_exp >> expected_seg_type
                    >> expected_aln_from >> expected_aln_to
                    >> expected_seq_from >> expected_seq_to;
                BOOST_CHECK(in_exp.good());
                BOOST_CHECK(seg.GetType() == expected_seg_type);
                BOOST_CHECK(aln_rg.GetFrom() == expected_aln_from);
                BOOST_CHECK(aln_rg.GetTo() == expected_aln_to);
                BOOST_CHECK(seq_rg.GetFrom() == expected_seq_from);
                BOOST_CHECK(seq_rg.GetTo() == expected_seq_to);
            }

            if ( check_sequence ) {
                string aln_sequence, row_sequence;
                string expected_aln_sequence, expected_row_sequence;

                if ( !aln_rg.Empty() ) {
                    sparse.GetAlnSeqString(row, aln_sequence, aln_rg);
                    if ( aln_sequence.empty() ) {
                        aln_sequence = kNoData;
                    }
                }
                else {
                    aln_sequence = kGap;
                }
                if ( !seq_rg.Empty() ) {
                    sparse.GetSeqString(row, row_sequence,
                        IAlnExplorer::TRange(seq_rg.GetFrom(), seq_rg.GetTo()));
                    if ( row_sequence.empty() ) {
                        row_sequence = kNoData;
                    }
                }
                else {
                    row_sequence = kGap;
                }

                if ( DumpExpected() ) {
                    cout << aln_sequence << endl;
                    cout << row_sequence << endl;
                }
                else {
                    in_exp >> ws;
                    getline(in_exp, expected_aln_sequence);
                    getline(in_exp, expected_row_sequence);
                    BOOST_CHECK(in_exp.good());
                    BOOST_CHECK(aln_sequence == expected_aln_sequence);
                    BOOST_CHECK(row_sequence == expected_row_sequence);
                }
            }
        }
    }
}


BOOST_AUTO_TEST_CASE(s_TestLoadAlign)
{
    cout << "Test CAlnContainer and CAlnStats..." << endl;

    CNcbiIfstream in_data("data/aligns1.asn");
    CNcbiIfstream in_exp("data/expected01.txt");
    // File format:
    // <int-number-of-alignments>
    // For each seq-id:
    // <int-base-width> <Seq-id>

    CAlnContainer aligns;
    size_t num_aligns = LoadAligns(in_data, aligns);

    if ( DumpExpected() ) {
        cout << num_aligns << endl;
    }
    else {
        size_t expected_num_aligns;
        in_exp >> expected_num_aligns;
        BOOST_CHECK(num_aligns == expected_num_aligns);
    }

    auto_ptr<TScopeAlnIdMap> aln_id_map;
    CRef<TScopeAlnStats> aln_stats = InitAlnStats(aligns, aln_id_map);

    if ( !DumpExpected() ) {
        BOOST_CHECK(aln_stats->GetAlnCount() == num_aligns);
        BOOST_CHECK(aln_stats->CanBeAnchored());
    }

    ITERATE(TScopeAlnStats::TIdVec, id_it, aln_stats->GetIdVec()) {
        if ( DumpExpected() ) {
            cout << (*id_it)->GetBaseWidth() << " ";
            cout << MSerial_AsnText << (*id_it)->GetSeqId();
        }
        else {
            int width;
            CSeq_id id;
            in_exp >> width;
            in_exp >> MSerial_AsnText >> id;
            BOOST_CHECK(id.Equals((*id_it)->GetSeqId()));
            BOOST_CHECK((*id_it)->GetBaseWidth() == width);
        }
    }
}


BOOST_AUTO_TEST_CASE(s_TestPairwiseAln)
{
    cout << "Test CPairwiseAln..." << endl;

    CNcbiIfstream in_data("data/aligns1.asn");
    CNcbiIfstream in_exp("data/expected02.txt");

    // File format:
    // <int-number-of-alignments>
    // see CheckPairwiseAln

    CAlnContainer aligns;
    size_t num_aligns = LoadAligns(in_data, aligns);

    if ( DumpExpected() ) {
        cout << num_aligns << endl;
    }
    else {
        size_t expected_num_aligns;
        in_exp >> expected_num_aligns;
        BOOST_CHECK(num_aligns == expected_num_aligns);
    }

    auto_ptr<TScopeAlnIdMap> aln_id_map;
    CRef<TScopeAlnStats> aln_stats = InitAlnStats(aligns, aln_id_map);

    const TScopeAlnStats::TAlnVec& aln_vec = aln_stats->GetAlnVec();

    // For each source alignment create a set of pairwise alignments.
    for (size_t aln_idx = 0; aln_idx < aln_vec.size(); ++aln_idx) {
        const CSeq_align& aln = *aln_vec[aln_idx];
        // Use row 0 as anchor, get pairwise alignments with all other rows.
        const TScopeAlnStats::TIdVec& aln_ids = aln_stats->GetSeqIdsForAln(aln_idx);
        // Test two different anchor values.
        for (int anchor = 0; anchor < 2; anchor++) {
            TAlnSeqIdIRef anchor_id = aln_ids[anchor];
            for (int row = 0; row < aln_stats->GetDimForAln(aln_idx); ++row) {
                if (row == anchor) continue;
                TAlnSeqIdIRef row_id = aln_ids[row];
                CPairwiseAln pw(anchor_id, row_id, CPairwiseAln::fDefaultPolicy);
                ConvertSeqAlignToPairwiseAln(pw, aln, anchor, row,
                    CAlnUserOptions::eBothDirections, &aln_ids);

                CheckPairwiseAln(in_exp, pw, aln_idx, anchor, row);

                for (CPairwise_CI seg(pw); seg; ++seg) {
                    if ( DumpExpected() ) {
                        cout << "  " << seg.GetFirstRange().GetFrom() << " "
                            << seg.GetFirstRange().GetTo() << " "
                            << seg.GetSecondRange().GetFrom() << " "
                            << seg.GetSecondRange().GetTo() << " "
                            << (seg.IsDirect() ? 1 : 0) << " "
                            << (seg.IsFirstDirect() ? 1 : 0) << " "
                            << (seg.GetSegType() == CPairwise_CI::eAligned ? 1 : 0) << endl;
                    }
                    else {
                        int direct, first_direct, aligned;
                        TSignedSeqPos first_from, first_to, second_from, second_to;
                        in_exp >> first_from >> first_to >> second_from >> second_to >>
                            direct >> first_direct >> aligned;
                        BOOST_CHECK(in_exp.good());
                        BOOST_CHECK(seg.GetFirstRange().GetFrom() == first_from);
                        BOOST_CHECK(seg.GetFirstRange().GetTo() == first_to);
                        BOOST_CHECK(seg.GetSecondRange().GetFrom() == second_from);
                        BOOST_CHECK(seg.GetSecondRange().GetTo() == second_to);
                        BOOST_CHECK(seg.IsDirect() == (direct != 0));
                        BOOST_CHECK(seg.IsFirstDirect() == (first_direct != 0));
                        BOOST_CHECK((seg.GetSegType() == CPairwise_CI::eAligned) == (aligned != 0));
                    }
                }
            }
        }
    }

}


BOOST_AUTO_TEST_CASE(s_TestPairwiseAlnRanged)
{
    cout << "Test ranged CPairwise_CI..." << endl;

    CNcbiIfstream in_data("data/aligns1.asn");
    CNcbiIfstream in_exp("data/expected03.txt");

    // File format:
    // <int-number-of-alignments>
    //
    // For each alignment/anchor/row:
    // <int-align-index> <int-anchor_index> <int-row-index> <int-pairwise-flags>
    //   <int-align-first-from> <int-align-first-to> <int-base-width> <Seq-id>
    //   <int-align-second-from> <int-align-second-to> <int-base-width> <Seq-id>
    //     For each CPairwiseAln element:
    //     <int-first-from> <int-second-from> <int-len> <int-is-direct>
    //   <int-insertions-count>
    //     For each insertion:
    //     <int-first-from> <int-second-from> <int-len> <int-is-direct>
    //   For each iterator:
    //   <int-first-from> <int-first-to> <int-second-from> <int-second-to> ...
    //   ... <int-direct> <int-first-direct> <int-aligned>

    CAlnContainer aligns;
    size_t num_aligns = LoadAligns(in_data, aligns);

    if ( DumpExpected() ) {
        cout << num_aligns << endl;
    }
    else {
        size_t expected_num_aligns;
        in_exp >> expected_num_aligns;
        BOOST_CHECK(num_aligns == expected_num_aligns);
    }

    auto_ptr<TScopeAlnIdMap> aln_id_map;
    CRef<TScopeAlnStats> aln_stats = InitAlnStats(aligns, aln_id_map);

    const TScopeAlnStats::TAlnVec& aln_vec = aln_stats->GetAlnVec();

    // For each source alignment create a set of pairwise alignments.
    for (size_t aln_idx = 0; aln_idx < aln_vec.size(); ++aln_idx) {
        const CSeq_align& aln = *aln_vec[aln_idx];
        // Use row 0 as anchor, get pairwise alignments with all other rows.
        const TScopeAlnStats::TIdVec& aln_ids = aln_stats->GetSeqIdsForAln(aln_idx);
        // Test two different anchor values.
        for (int anchor = 0; anchor < 2; anchor++) {
            TAlnSeqIdIRef anchor_id = aln_ids[anchor];
            for (int row = 0; row < aln_stats->GetDimForAln(aln_idx); ++row) {
                if (row == anchor) continue;
                TAlnSeqIdIRef row_id = aln_ids[row];
                CPairwiseAln pw(anchor_id, row_id, CPairwiseAln::fDefaultPolicy);
                ConvertSeqAlignToPairwiseAln(pw, aln, anchor, row,
                    CAlnUserOptions::eBothDirections, &aln_ids);

                TSignedSeqPos first_from, first_to, second_from, second_to;

                if ( DumpExpected() ) {
                    cout << aln_idx << " " << anchor << " " << row << " " << pw.GetFlags() << endl;
                    cout << "  " << pw.GetFirstFrom() << " "
                        << pw.GetFirstTo() << " "
                        << pw.GetFirstBaseWidth() << " "
                        << MSerial_AsnText << pw.GetFirstId()->GetSeqId();
                    cout << "  " << pw.GetSecondPosByFirstPos(pw.GetFirstFrom()) << " "
                        << pw.GetSecondPosByFirstPos(pw.GetFirstTo()) << " "
                        << pw.GetSecondBaseWidth() << " "
                        << MSerial_AsnText << pw.GetSecondId()->GetSeqId();
                }
                else {
                    size_t expected_aln_idx;
                    int expected_row, expected_anchor;
                    int first_width, second_width, flags;
                    CSeq_id first_id, second_id;
                    in_exp >> expected_aln_idx >> expected_anchor >> expected_row >> flags;
                    in_exp >> first_from >> first_to >> first_width;
                    in_exp >> MSerial_AsnText >> first_id;
                    in_exp >> second_from >> second_to >> second_width;
                    in_exp >> MSerial_AsnText >> second_id;
                    BOOST_CHECK(in_exp.good());
                    BOOST_CHECK(aln_idx == expected_aln_idx);
                    BOOST_CHECK(anchor == expected_anchor);
                    BOOST_CHECK(row == expected_row);
                    BOOST_CHECK(pw.GetFlags() == flags);
                    BOOST_CHECK(pw.GetFirstId()->GetSeqId().Equals(first_id));
                    BOOST_CHECK(pw.GetSecondId()->GetSeqId().Equals(second_id));
                    BOOST_CHECK(pw.GetFirstBaseWidth() == first_width);
                    BOOST_CHECK(pw.GetSecondBaseWidth() == second_width);
                    BOOST_CHECK(pw.GetFirstFrom() == first_from);
                    BOOST_CHECK(pw.GetFirstTo() == first_to);
                    BOOST_CHECK(pw.GetSecondPosByFirstPos(pw.GetFirstFrom()) == second_from);
                    BOOST_CHECK(pw.GetSecondPosByFirstPos(pw.GetFirstTo()) == second_to);
                }

                if (pw.size() < 2) continue;

                first_from = pw[1].GetFirstFrom() + 1;
                first_to = pw[pw.size() - 1].GetFirstFrom() - 2;
                CPairwise_CI::TSignedRange rg(first_from, first_to);

                for (CPairwise_CI seg(pw, rg); seg; ++seg) {
                    if ( DumpExpected() ) {
                        cout << "  " << seg.GetFirstRange().GetFrom() << " "
                            << seg.GetFirstRange().GetTo() << " "
                            << seg.GetSecondRange().GetFrom() << " "
                            << seg.GetSecondRange().GetTo() << " "
                            << (seg.IsDirect() ? 1 : 0) << " "
                            << (seg.IsFirstDirect() ? 1 : 0) << " "
                            << (seg.GetSegType() == CPairwise_CI::eAligned ? 1 : 0) << endl;
                    }
                    else {
                        int direct, first_direct, aligned;
                        in_exp >> first_from >> first_to >> second_from >> second_to >>
                            direct >> first_direct >> aligned;
                        BOOST_CHECK(in_exp.good());
                        BOOST_CHECK(seg.GetFirstRange().GetFrom() == first_from);
                        BOOST_CHECK(seg.GetFirstRange().GetTo() == first_to);
                        BOOST_CHECK(seg.GetSecondRange().GetFrom() == second_from);
                        BOOST_CHECK(seg.GetSecondRange().GetTo() == second_to);
                        BOOST_CHECK(seg.IsDirect() == (direct != 0));
                        BOOST_CHECK(seg.IsFirstDirect() == (first_direct != 0));
                        BOOST_CHECK((seg.GetSegType() == CPairwise_CI::eAligned) == (aligned != 0));
                    }
                }
            }
        }
    }
}


BOOST_AUTO_TEST_CASE(s_TestAnchoredAln)
{
    cout << "Test CAnchoredAln..." << endl;

    CNcbiIfstream in_data("data/aligns1.asn");
    CNcbiIfstream in_exp("data/expected04.txt");

    CAlnContainer aligns;
    size_t num_aligns = LoadAligns(in_data, aligns);

    if ( DumpExpected() ) {
        cout << num_aligns << endl;
    }
    else {
        size_t expected_num_aligns;
        in_exp >> expected_num_aligns;
        BOOST_CHECK(num_aligns == expected_num_aligns);
    }

    auto_ptr<TScopeAlnIdMap> aln_id_map;
    CRef<TScopeAlnStats> aln_stats = InitAlnStats(aligns, aln_id_map);

    for (size_t aln_idx = 0; aln_idx < aligns.size(); ++aln_idx) {
        for (int anchor = 0; anchor < aln_stats->GetDimForAln(aln_idx); ++anchor) {
            CAlnUserOptions user_options;
            CRef<CAnchoredAln> anchored_aln = CreateAnchoredAlnFromAln(*aln_stats,
                aln_idx, user_options, anchor);

            for (int row = 0; row < (int)anchored_aln->GetPairwiseAlns().size(); ++row) {
                CheckPairwiseAln(in_exp,
                    *anchored_aln->GetPairwiseAlns()[row],
                    aln_idx, anchored_aln->GetAnchorRow(), row);
            }
        }
    }
}


BOOST_AUTO_TEST_CASE(s_TestAnchoredAlnBuilt)
{
    static const int kMergeFlags[] = {
        0,
        CAlnUserOptions::fTruncateOverlaps,
        CAlnUserOptions::fAllowTranslocation,
        CAlnUserOptions::fUseAnchorAsAlnSeq,
        CAlnUserOptions::fAnchorRowFirst,
        CAlnUserOptions::fIgnoreInsertions
    };

    cout << "Test built CAnchoredAln..." << endl;

    CNcbiIfstream in_data("data/aligns2.asn");
    CNcbiIfstream in_exp("data/expected05.txt");

    while (!in_data.eof()  &&  in_data.good()) {
        size_t num_to_merge;
        in_data >> num_to_merge;
        if (num_to_merge == 0  ||  !in_data.good()) break;

        CAlnContainer aligns;
        size_t num_aligns = LoadAligns(in_data, aligns, num_to_merge);

        if ( DumpExpected() ) {
            cout << num_aligns << endl;
        }
        else {
            size_t expected_num_aligns;
            in_exp >> expected_num_aligns;
            BOOST_CHECK(num_aligns == expected_num_aligns);
        }

        auto_ptr<TScopeAlnIdMap> aln_id_map;
        CRef<TScopeAlnStats> aln_stats = InitAlnStats(aligns, aln_id_map);

        const TScopeAlnStats::TIdVec& aln_ids = aln_stats->GetAnchorIdVec();
        for (size_t anchor_idx = 0; anchor_idx < aln_ids.size(); anchor_idx++) {
            TAlnSeqIdIRef anchor_id = aln_ids[anchor_idx];
            CAlnUserOptions user_options;
            user_options.SetAnchorId(anchor_id);

            for (size_t flags_idx = 0; flags_idx < sizeof(kMergeFlags)/sizeof(kMergeFlags[0]); flags_idx++) {
                user_options.m_MergeFlags = kMergeFlags[flags_idx];

                if ( DumpExpected() ) {
                    cout << anchor_idx << " "
                        << user_options.m_MergeFlags << endl;
                }
                else {
                    size_t expected_anchor_idx;
                    int expected_flags;
                    in_exp >> expected_anchor_idx >> expected_flags;
                    BOOST_CHECK(in_exp.good());
                    BOOST_CHECK(anchor_idx == expected_anchor_idx);
                    BOOST_CHECK(user_options.m_MergeFlags == expected_flags);
                }

                TAnchoredAlnVec anchored_aln_vec;
                CreateAnchoredAlnVec(*aln_stats, anchored_aln_vec, user_options);

                CRef<CSeq_id> id(new CSeq_id);
                id->SetLocal().SetStr("pseudo-id");
                TAlnSeqIdIRef pseudo_id(Ref(new CAlnSeqId(*id)));
                CAnchoredAln built_aln;
                BuildAln(anchored_aln_vec, built_aln, user_options, pseudo_id);

                for (int row = 0; row < (int)built_aln.GetPairwiseAlns().size(); ++row) {
                    CheckPairwiseAln(in_exp,
                        *built_aln.GetPairwiseAlns()[row],
                        0, built_aln.GetAnchorRow(), row);
                }
            }
        }
    }
}


BOOST_AUTO_TEST_CASE(s_TestSparseAlnCoords)
{
    cout << "Test CSparseAln coordinates..." << endl;

    CNcbiIfstream in_data("data/aligns2.asn");
    CNcbiIfstream in_exp("data/expected06.txt");

    while (!in_data.eof()  &&  in_data.good()) {
        size_t num_to_merge;
        in_data >> num_to_merge;
        if (num_to_merge == 0  ||  !in_data.good()) break;

        CAlnContainer aligns;
        size_t num_aligns = LoadAligns(in_data, aligns, num_to_merge);

        if ( DumpExpected() ) {
            cout << num_aligns << endl;
        }
        else {
            size_t expected_num_aligns;
            in_exp >> expected_num_aligns;
            BOOST_CHECK(num_aligns == expected_num_aligns);
        }

        auto_ptr<TScopeAlnIdMap> aln_id_map;
        CRef<TScopeAlnStats> aln_stats = InitAlnStats(aligns, aln_id_map);

        const TScopeAlnStats::TIdVec& aln_ids = aln_stats->GetAnchorIdVec();
        for (size_t anchor_idx = 0; anchor_idx < aln_ids.size(); anchor_idx++) {
            TAlnSeqIdIRef anchor_id = aln_ids[anchor_idx];
            CAlnUserOptions user_options;
            user_options.SetAnchorId(anchor_id);
            user_options.m_MergeFlags = CAlnUserOptions::fTruncateOverlaps;

            if ( DumpExpected() ) {
                cout << anchor_idx << " "
                    << MSerial_AsnText << anchor_id->GetSeqId();
            }
            else {
                size_t expected_anchor_idx;
                in_exp >> expected_anchor_idx;
                CSeq_id expected_anchor_id;
                in_exp >> MSerial_AsnText >> expected_anchor_id;
                BOOST_CHECK(in_exp.good());
                BOOST_CHECK(anchor_idx == expected_anchor_idx);
                BOOST_CHECK(anchor_id->GetSeqId().Equals(expected_anchor_id));
            }

            TAnchoredAlnVec anchored_aln_vec;
            CreateAnchoredAlnVec(*aln_stats, anchored_aln_vec, user_options);

            CRef<CSeq_id> id(new CSeq_id);
            id->SetLocal().SetStr("pseudo-id");
            TAlnSeqIdIRef pseudo_id(Ref(new CAlnSeqId(*id)));
            CAnchoredAln built_aln;
            BuildAln(anchored_aln_vec, built_aln, user_options, pseudo_id);

            CSparseAln sparse_aln(built_aln, GetScope());

            CheckSparseAln(in_exp, sparse_aln, false);
        }
    }
}


BOOST_AUTO_TEST_CASE(s_TestSparseAlnData)
{
    cout << "Test CSparseAln sequence..." << endl;

    CNcbiIfstream in_data("data/aligns3.asn");
    CNcbiIfstream in_exp("data/expected07.txt");

    while (!in_data.eof()  &&  in_data.good()) {
        size_t num_to_merge;
        in_data >> num_to_merge;
        if (num_to_merge == 0  ||  !in_data.good()) break;

        CAlnContainer aligns;
        size_t num_aligns = LoadAligns(in_data, aligns, num_to_merge);

        if ( DumpExpected() ) {
            cout << num_aligns << endl;
        }
        else {
            size_t expected_num_aligns;
            in_exp >> expected_num_aligns;
            BOOST_CHECK(num_aligns == expected_num_aligns);
        }

        auto_ptr<TScopeAlnIdMap> aln_id_map;
        CRef<TScopeAlnStats> aln_stats = InitAlnStats(aligns, aln_id_map);

        const TScopeAlnStats::TIdVec& aln_ids = aln_stats->GetAnchorIdVec();
        for (size_t anchor_idx = 0; anchor_idx < aln_ids.size(); anchor_idx++) {
            TAlnSeqIdIRef anchor_id = aln_ids[anchor_idx];
            CAlnUserOptions user_options;
            user_options.SetAnchorId(anchor_id);
            user_options.m_MergeFlags = CAlnUserOptions::fTruncateOverlaps;

            if ( DumpExpected() ) {
                cout << anchor_idx << " "
                    << MSerial_AsnText << anchor_id->GetSeqId();
            }
            else {
                size_t expected_anchor_idx;
                in_exp >> expected_anchor_idx;
                CSeq_id expected_anchor_id;
                in_exp >> MSerial_AsnText >> expected_anchor_id;
                BOOST_CHECK(in_exp.good());
                BOOST_CHECK(anchor_idx == expected_anchor_idx);
                BOOST_CHECK(anchor_id->GetSeqId().Equals(expected_anchor_id));
            }

            TAnchoredAlnVec anchored_aln_vec;
            CreateAnchoredAlnVec(*aln_stats, anchored_aln_vec, user_options);

            CRef<CSeq_id> id(new CSeq_id);
            id->SetLocal().SetStr("pseudo-id");
            TAlnSeqIdIRef pseudo_id(Ref(new CAlnSeqId(*id)));
            CAnchoredAln built_aln;
            BuildAln(anchored_aln_vec, built_aln, user_options, pseudo_id);

            CSparseAln sparse_aln(built_aln, GetScope());

            CheckSparseAln(in_exp, sparse_aln, true);

            CRef<CSeq_align> aln = CreateSeqAlignFromAnchoredAln(built_aln, CSeq_align::TSegs::e_Denseg, &GetScope());
            BOOST_CHECK(aln);

            if ( DumpExpected() ) {
                cout << MSerial_AsnText << *aln;
            }
            else {
                CSeq_align expected_aln;
                in_exp >> MSerial_AsnText >> expected_aln;
                BOOST_CHECK(aln->Equals(expected_aln));
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(s_TestAlnTypes)
{
    cout << "Test seq-align types..." << endl;

    CNcbiIfstream in_data("data/aligns4.asn");
    CNcbiIfstream in_exp("data/expected08.txt");

    CAlnContainer aligns;
    size_t num_aligns = LoadAligns(in_data, aligns);

    if ( DumpExpected() ) {
        cout << num_aligns << endl;
    }
    else {
        size_t expected_num_aligns;
        in_exp >> expected_num_aligns;
        BOOST_CHECK(num_aligns == expected_num_aligns);
    }

    auto_ptr<TScopeAlnIdMap> aln_id_map;
    CRef<TScopeAlnStats> aln_stats = InitAlnStats(aligns, aln_id_map);

    for (size_t aln_idx = 0; aln_idx < aligns.size(); ++aln_idx) {
        for (int anchor = 0; anchor < aln_stats->GetDimForAln(aln_idx); ++anchor) {

            // Special case - sparse-segs can be anchored only to the first row.
            if (aln_stats->GetAlnVec()[aln_idx]->GetSegs().IsSparse()  &&  anchor > 0) {
                break;
            }

            CAlnUserOptions user_options;
            CRef<CAnchoredAln> anchored_aln = CreateAnchoredAlnFromAln(*aln_stats,
                aln_idx, user_options, anchor);

            for (int row = 0; row < (int)anchored_aln->GetPairwiseAlns().size(); ++row) {
                CheckPairwiseAln(in_exp,
                    *anchored_aln->GetPairwiseAlns()[row],
                    aln_idx, anchored_aln->GetAnchorRow(), row);
            }
        }
    }
}
