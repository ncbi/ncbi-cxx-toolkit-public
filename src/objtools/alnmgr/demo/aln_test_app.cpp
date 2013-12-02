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
* Author:  Kamen Todorov
*
* File Description:
*   Demo of Seq-align tests.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>

//#include <common/test_assert.h>

#include <serial/objistr.hpp>

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

using namespace ncbi;
using namespace objects;


/// Types we use here:
typedef CSeq_align::TDim TDim;
typedef vector<const CSeq_align*> TAlnVector;
typedef const CSeq_id* TSeqIdPtr;
typedef vector<TSeqIdPtr> TSeqIdVector;


class CAlnTestApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);

    CScope& GetScope(void) const;
    void LoadInputAlns(const string& file_name, const string& asn_type);

    bool InsertAln(const CSeq_align* aln)
    {
        aln->Validate(true);
        m_AlnContainer.insert(*aln);
        return true;
    }

private:
    mutable CRef<CObjectManager> m_ObjMgr;
    mutable CRef<CScope>         m_Scope;
    CAlnContainer                m_AlnContainer;
};


void CAlnTestApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddDefaultKey("in", "InputFileName",
        "Name of file to read from (standard input by default)",
        CArgDescriptions::eInputFile, "-");

    arg_desc->AddDefaultKey("b", "bin_obj_type",
        "This forces the input file to be read in binary ASN.1 mode\n"
        "and specifies the type of the top-level ASN.1 object.\n",
        CArgDescriptions::eString, "");

    arg_desc->AddDefaultKey("anchor_row", "AnchorRow",
        "Anchor row index",
        CArgDescriptions::eInteger, "-1");

    arg_desc->AddOptionalKey("anchor_id", "AnchorSeqId",
        "Anchor seq-id (fasta or ASN.1)",
        CArgDescriptions::eString);

    arg_desc->AddOptionalKey("pseudo_id", "PseudoSeqId",
        "Pseudo seq-id (fasta or ASN.1) to be used as anchor of merged CAnchoredAln",
        CArgDescriptions::eString);

    arg_desc->AddFlag("print_seq",
        "Print sequence data");

    arg_desc->AddFlag("no_scope",
        "Don't use CScope while loading alignments");

    arg_desc->AddFlag("ignore_gaps",
        "Ignore gaps (insertions) when collecting alignments");

    arg_desc->AddFlag("merge_keep_overlaps",
        "Don't truncate overlaps when merging alignments");

    arg_desc->AddFlag("merge_align_to_anchor",
        "Use anchor row as the alignment row "
        "(don't translate to alignment coordinates)");

    arg_desc->AddFlag("merge_ignore_gaps",
        "Ignore gaps (insertions) when merging alignments");

    // Program description
    string prog_description = "Seq-align Test Application.\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    SetupArgDescriptions(arg_desc.release());
}


void CAlnTestApp::LoadInputAlns(const string& file_name, const string& asn_type)
{
    bool binary = !asn_type.empty();
    auto_ptr<CObjectIStream> in(CObjectIStream::Open(
        binary ? eSerial_AsnBinary : eSerial_AsnText, file_name));

    CAlnAsnReader reader(&GetScope());
    reader.Read(in.get(),
        bind1st(mem_fun(&CAlnTestApp::InsertAln), this),
        asn_type);
}


CScope& CAlnTestApp::GetScope(void) const
{
    if (!m_Scope) {
        m_ObjMgr = CObjectManager::GetInstance();
        CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);
        
        m_Scope = new CScope(*m_ObjMgr);
        m_Scope->AddDefaults();
    }
    return *m_Scope;
}


void DumpPairwiseAln(const CPairwiseAln& pw, const string ind)
{
    cout << ind << "CPairwiseAln" << endl;
    cout << ind << "  First-id: " << pw.GetFirstId()->AsString() << " dir = ";
    if (pw.GetFlags() & CAlignRangeCollection<CAlignRange<TSignedSeqPos> >::fDirect) cout << "D";
    if (pw.GetFlags() & CAlignRangeCollection<CAlignRange<TSignedSeqPos> >::fReversed) cout << "R";
    cout << endl;
    cout << ind << "  Second: " << pw.GetSecondId()->AsString() << " / "
        << pw.GetSecondPosByFirstPos(pw.GetFirstFrom()) << endl;
    ITERATE(CPairwiseAln, rit, pw) {
        CAlignRange<TSignedSeqPos> rg = *rit;
        cout << ind << "    " << rit->GetFirstFrom() << (rg.IsFirstDirect() ? "D" : "R") << " => "
            << rit->GetSecondFrom() << (rg.IsDirect() ? "d" : "r") <<
            " len=" << rit->GetLength() << endl;
    }
    if ( !pw.GetInsertions().empty() ) {
        cout << ind << "  Insertions on " << pw.GetFirstId()->AsString() << ":" << endl;
        ITERATE(CPairwiseAln::TAlignRangeVector, gap, pw.GetInsertions()) {
            cout << ind << "    [" <<
                gap->GetFirstFrom() << (gap->IsFirstDirect() ? "D" : "R") <<
                "] => " << gap->GetSecondFrom() <<
                (gap->IsDirect() ? "d" : "r") <<
                " len=" << gap->GetLength() << endl;
        }
    }
    cout << ind << "  CPairwise_CI" << endl;
    CPairwise_CI::TSignedRange rg = pw.GetFirstRange();
    rg.SetToOpen(rg.GetToOpen() + 10);
    for (CPairwise_CI it(pw, rg); it; ++it) {
        cout << ind << "    " <<
            it.GetFirstRange().GetFrom() << " - " << it.GetFirstRange().GetToOpen() <<
            (it.IsFirstDirect() ? " D" : " R") <<
            " => " << it.GetSecondRange().GetFrom() << " - " << it.GetSecondRange().GetToOpen() <<
            (it.IsDirect() ? " d" : " r") <<
            " [" << it.GetFirstRange().GetLength() << "] " <<
            (it.GetSegType() == CPairwise_CI::eGap ? "Gap" : "Aligned") << endl;
    }
}


void DumpAnchoredAln(const CAnchoredAln& aaln, const string& ind)
{
    cout << ind << "CAnchoredAln" << endl;
    ITERATE(CAnchoredAln::TPairwiseAlnVector, pit, aaln.GetPairwiseAlns()) {
        DumpPairwiseAln(**pit, ind + "  ");
    }
}


static bool s_PrintSeq = false;


void DumpSparseAln(const CSparseAln& sparse_aln)
{
    cout << "CSparseAln" << endl;
    cout << "  Rows: " << sparse_aln.GetNumRows() << endl;
    cout << "  Anchor row: " << sparse_aln.GetAnchor() << endl;
    cout << "  Aln range: " << sparse_aln.GetAlnRange().GetFrom() << " - "
        << sparse_aln.GetAlnRange().GetTo() << endl;
    for (CSparseAln::TDim row = 0;  row < sparse_aln.GetDim(); ++row) {
        cout << "  Row " << row << ": "
             << sparse_aln.GetSeqId(row).AsFastaString() << endl;
        CSparseAln::TRange rg = sparse_aln.GetSeqRange(row);
        CSparseAln::TRange native_rg = sparse_aln.AlnRangeToNativeSeqRange(row, rg);
        CSparseAln::TFrames frames = sparse_aln.AlnRangeToNativeFrames(row, rg);
        cout << "    AlnRg: " << sparse_aln.GetSeqAlnStart(row) << " - " << sparse_aln.GetSeqAlnStop(row) << endl;
        if (sparse_aln.GetBaseWidth(row) != 1) {
            cout << "    SeqRg: " << native_rg.GetFrom() << "/" << frames.first <<
                " - " << native_rg.GetTo() << "/" << frames.second <<
                " (" << rg.GetFrom() << " - " << rg.GetTo() << ")";
        }
        else {
            cout << "    SeqRg: " << rg.GetFrom() << " - " << rg.GetTo();
        }
        cout << endl;

        CSparse_CI sparse_ci;

        if ( s_PrintSeq ) {
            try {
                string sequence;
                sparse_aln.GetAlnSeqString(row, sequence, sparse_aln.GetSeqAlnRange(row));
                cout << "    Aln: " << sequence << endl;
                sequence.clear();
                sparse_aln.GetSeqString(row, sequence, sparse_aln.GetSeqRange(row), false);
                cout << "    Seq: " << sequence << endl;
            } catch (...) {
                // if sequence is not in scope,
                // the above is impossible
            }

            // Aligned sequence segments
            sparse_ci = CSparse_CI(sparse_aln, row, CSparse_CI::eAllSegments);

            string aln_total, seq_total;
            while (sparse_ci) {
                try {
                    const IAlnSegment& seg = *sparse_ci;
                    CSparse_CI::TSignedRange aln_rg = seg.GetAlnRange();
                    CSparse_CI::TSignedRange seq_rg = seg.GetRange();
                    string aln_data, seq_data, aln_gap, seq_gap;
                    if ((seg.GetType() & CSparseSegment::fUnaligned) == 0) {
                        sparse_aln.GetAlnSeqString(row, aln_data, aln_rg);
                    }
                    if ((seg.GetType() & CSparseSegment::fUnaligned) == 0  &&
                        !seq_rg.Empty()) {
                        sparse_aln.GetSeqString(row, seq_data,
                            IAlnExplorer::TRange(seq_rg.GetFrom(), seq_rg.GetTo()),
                            // Force translation for mixed alignments only.
                            sparse_aln.GetBaseWidth(row) != 1);
                    }
                    if ((seg.GetType() & (CSparseSegment::fIndel | CSparseSegment::fGap)) != 0) {
                        if ( aln_rg.Empty() ) {
                            aln_gap = string(seq_data.size(), '*');
                        }
                        if ( seq_rg.Empty() ) {
                            seq_gap = string(aln_data.size(), '?');
                        }
                    }
                    if (!aln_data.empty()) {
                        aln_total.append(aln_data);
                        aln_total.append(aln_gap);
                        seq_total.append(seq_gap);
                        seq_total.append(seq_data);
                    }
                }
                catch (...) {}
                ++sparse_ci;
            }
            if (!aln_total.empty()  ||  !seq_total.empty()) {
                cout << "    Aligned:" << endl;
                cout << "       " << aln_total << endl;
                cout << "       " << seq_total << endl;
            }
            else {
                cout << "    [No sequence data]" << endl;
            }
        }

        cout << "    Segments:" << endl;
        // Row segments
        sparse_ci = CSparse_CI(sparse_aln, row, IAlnSegmentIterator::eAllSegments);

        while (sparse_ci) {
            cout << "     " << *sparse_ci << endl;
            ++sparse_ci;
        }
    }
    cout << endl;
}


void TestSparseIt(const CSparseAln& aln, int row, const CSparse_CI::TSignedRange& rg, CSparse_CI::EFlags flags)
{
    for (CSparse_CI it(aln, row, flags, rg); it; ++it) {
        cout << "  "
            << it->GetAlnRange().GetFrom() << " - "
            << it->GetAlnRange().GetTo() << " (len="
            << it->GetAlnRange().GetLength() << ") "
            << (it.IsAnchorDirect() ? "D" : "R")
            << " -> "
            << it->GetRange().GetFrom() << " - "
            << it->GetRange().GetTo() << " (len="
            << it->GetRange().GetLength() << ") "
            << (it->IsReversed() ? "R" : "D")
            << " | ";
        switch (it->GetType() & CSparseSegment::fSegTypeMask) {
        case CSparseSegment::fAligned:
            cout << "Aligned";
            break;
        case CSparseSegment::fIndel:
            cout << "Indel";
            break;
        case CSparseSegment::fGap:
            cout << "Gap";
            break;
        case CSparseSegment::fUnaligned:
            cout << "Unaligned";
            break;
        }
        if (aln.GetAnchor() != row) {
            TSignedSeqPos anchor_from = aln.GetSeqPosFromAlnPos(
                aln.GetAnchor(), it->GetAlnRange().GetFrom(),
                CSparseAln::eForward, true);
            TSignedSeqPos anchor_to = aln.GetSeqPosFromAlnPos(
                aln.GetAnchor(), it->GetAlnRange().GetTo(),
                CSparseAln::eForward, true);
            cout << " [anchor range: " << anchor_from << " - " << anchor_to << "]";
        }
        cout << endl;
    }
}


void TestIterators(const CSparseAln& aln, const CSparse_CI::TSignedRange& rg)
{
    cout << "CSparse_CI:" << endl;
    int anchor_row = aln.GetAnchor();
    for (int row = 0; row < aln.GetNumRows(); ++row) {
        if (row == anchor_row) {
            cout << "anchor segments, id=";
        }
        else {
            cout << "row " << row << " segments, id=";
        }
        cout << aln.GetSeqId(row).AsFastaString() << endl;
        //cout << "Type: " << (aln.GetBioseqHandle(row).IsNa() ? "NA" : "AA") << endl;
        cout << "AllSegments:" << endl;
        TestSparseIt(aln, row, rg, CSparse_CI::eAllSegments);
        cout << "SkipGaps:" << endl;
        TestSparseIt(aln, row, rg, CSparse_CI::eSkipGaps);
        cout << "SkipInserts:" << endl;
        TestSparseIt(aln, row, rg, CSparse_CI::eSkipInserts);
        cout << "InsertsOnly:" << endl;
        TestSparseIt(aln, row, rg, CSparse_CI::eInsertsOnly);
        cout << endl;
    }
    cout << endl;
}


void TestSparseIterator(const CSparseAln& aln)
{
    CSparse_CI::TSignedRange rg = CSparse_CI::TSignedRange::GetWhole();
    TestIterators(aln, rg);
}


TAlnSeqIdIRef ArgToSeq_id(const CArgValue& arg)
{
    TAlnSeqIdIRef ret;
    if ( !arg ) {
        return ret;
    }
    const string& str = arg.AsString();
    if ( str.empty() ) {
        return ret;
    }

    CRef<CSeq_id> id;
    CNcbiIstrstream in(str.c_str());
    try {
        id.Reset(new CSeq_id());
        in >> MSerial_AsnText >> *id;
    }
    catch (...) {
        id.Reset();
    }
    if ( !id ) {
        // Not an ASN.1 seq-id
        id.Reset(new CSeq_id(str));
    }
    if (id  &&  id->Which() != CSeq_id::e_not_set) {
        ret.Reset(Ref(new CAlnSeqId(*id)));
    }
    return ret;
}


int CAlnTestApp::Run(void)
{
    const CArgs& args = GetArgs();
    string file_name = args["in"].AsString();
    // get the asn type of the top-level object
    string asn_type = args["b"].AsString();

    int anchor_row_idx = args["anchor_row"].AsInteger();

    LoadInputAlns(file_name, asn_type);

    s_PrintSeq = args["print_seq"];

    TScopeAlnSeqIdConverter id_conv(args["no_scope"] ? 0 : &GetScope());
    TScopeIdExtract id_extract(id_conv);
    TScopeAlnIdMap aln_id_map(id_extract, m_AlnContainer.size());
    TAlnSeqIdVec ids;
    ITERATE(CAlnContainer, aln_it, m_AlnContainer) {
        TAlnSeqIdVec tmp;
        id_extract(**aln_it, tmp);
        ids.insert(ids.end(), tmp.begin(), tmp.end());
        try {
            aln_id_map.push_back(**aln_it);
        }
        catch (CAlnException e) {
            cerr << "Skipping alignment: " << e.what() << endl;
        }
    }
    TScopeAlnStats aln_stats(aln_id_map);

    cout << "Collected ids:" << endl;
    ITERATE(TAlnSeqIdVec, id, ids) {
        cout << "  " << (*id)->AsString() << " W=" << (*id)->GetBaseWidth() << endl;
    }
    cout << endl;

    int pw_flags = CPairwiseAln::fDefaultPolicy;
    CAlnUserOptions aln_user_options;
    if ( args["ignore_gaps"] ) {
        aln_user_options.SetMergeFlags(CAlnUserOptions::fIgnoreInsertions, true);
        pw_flags |= CPairwiseAln::fIgnoreInsertions;
    }
    for (int i = 0; i < (int)aln_id_map.size(); i++) {
        cout << "Alignment #" << i << endl;
        TAlnSeqIdIRef id1(Ref(new CAlnSeqId(
            dynamic_cast<const CAlnSeqId&>(*aln_id_map[i][0]))));
        TAlnSeqIdIRef id2(Ref(new CAlnSeqId(
            dynamic_cast<const CAlnSeqId&>(*aln_id_map[i][1]))));
        CPairwiseAln pw(id1, id2, pw_flags);
        ConvertSeqAlignToPairwiseAln(pw, **m_AlnContainer.begin(), 0, 1,
            CAlnUserOptions::eBothDirections, &ids);
        cout << "Pairwise - row 0 vs 1:" << endl;
        DumpPairwiseAln(pw, "");

        CRef<CAnchoredAln> anchored_aln = CreateAnchoredAlnFromAln(aln_stats,
            i, aln_user_options, anchor_row_idx);
        cout << "Anchored to row " << anchored_aln->GetAnchorRow() << endl;
        DumpAnchoredAln(*anchored_aln, "");
        cout << endl;
        cout << "Sparse (unbuilt)" << endl;
        CSparseAln sparse_aln(*anchored_aln, GetScope());
        DumpSparseAln(sparse_aln);
        cout << endl;
    }

    CAlnUserOptions merge_options;
    if ( !args["merge_keep_overlaps"] ) {
        merge_options.SetMergeFlags(CAlnUserOptions::fTruncateOverlaps, true);
    }
    if ( args["merge_align_to_anchor"] ) {
        merge_options.SetMergeFlags(CAlnUserOptions::fUseAnchorAsAlnSeq, true);
    }
    if ( args["merge_ignore_gaps"] ) {
        merge_options.SetMergeFlags(CAlnUserOptions::fIgnoreInsertions, true);
    }

    TAlnSeqIdIRef anchor_id = ArgToSeq_id(args["anchor_id"]);
    if ( anchor_id ) {
        merge_options.SetAnchorId(anchor_id);
    }
    TAlnSeqIdIRef pseudo_id = ArgToSeq_id(args["pseudo_id"]);

    TAnchoredAlnVec anchored_aln_vec;
    merge_options.m_Direction = CAlnUserOptions::eBothDirections;
    CreateAnchoredAlnVec(aln_stats, anchored_aln_vec, merge_options);

    cout << "AnchoredAlnVec start" << endl;
    ITERATE(TAnchoredAlnVec, it, anchored_aln_vec) {
        DumpAnchoredAln(**it, "  ");
    }
    cout << "AnchoredAlnVec end" << endl << endl;

    CAnchoredAln built_aln;
    //merge_options.m_MergeAlgo = CAlnUserOptions::ePreserveRows;
    BuildAln(anchored_aln_vec, built_aln, merge_options, pseudo_id);

    cout << "BuildAln result" << endl;
    DumpAnchoredAln(built_aln, "");

    CSparseAln sparse_aln(built_aln, GetScope());
    DumpSparseAln(sparse_aln);
    TestSparseIterator(sparse_aln);

    for (int i = 0; i < sparse_aln.GetDim(); ++i) {
        cout << "row " << i << endl;
        IAlnExplorer::TSignedRange rg = sparse_aln.GetSeqAlnRange(i);
        IAlnSegmentIterator* it = sparse_aln.CreateSegmentIterator(i,
            rg,
            IAlnSegmentIterator::eAllSegments);
        while (*it) {
            CSparseAln::TSignedRange it_aln_rg = (*it)->GetAlnRange();
            CSparseAln::TSignedRange it_rg = (*it)->GetRange();
            CSparseAln::TSignedRange native_rg = sparse_aln.AlnRangeToNativeSeqRange(i, it_rg);

            cout << ((*it)->IsAligned() ? "  aligned : " : "  gap : ") <<
                it_aln_rg.GetFrom() << " - " <<
                it_aln_rg.GetTo() << " => " <<
                it_rg.GetFrom() << " - " <<
                it_rg.GetTo();
            if (sparse_aln.GetBaseWidth(i) != 1) {
                pair<int, int> fr = sparse_aln.AlnRangeToNativeFrames(i, it_rg);
                cout << " (" << native_rg.GetFrom() << "/" << fr.first <<
                    " - " << native_rg.GetTo() << "/" << fr.second << ")";
            }
            cout <<((*it)->IsReversed() ? " R" : " D") << endl;
            ++(*it);
        }
    }
    return 0;
}


int main(int argc, const char* argv[])
{
    return CAlnTestApp().AppMain(argc, argv);
}
