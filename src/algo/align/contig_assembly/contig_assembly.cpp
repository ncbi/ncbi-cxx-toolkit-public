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
 * Authors:  Josh Cherry
 *
 * File Description:  Alignment functions intended for use in contig assembly
 *
 */

#include <ncbi_pch.hpp>
#include <algo/align/contig_assembly/contig_assembly.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>
#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <algo/align/nw/nw_band_aligner.hpp>
#include <algo/align/nw/align_exception.hpp>
#include <objtools/alnmgr/alnvec.hpp>
#include <algo/dustmask/symdust.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(blast);

// Split a blast parameter string such as
// "-W 28 -r 1 -q -3 -e 1e-5 -Z 200 -F 'm L; R -d rodents.lib'"
// into ["-W", "28",..."-F", "m L; R -d rodents.lib"]
// (i.e., respect single quotes)
static void s_SplitCommandLine(string s, vector<string>& result)
{
    bool in_quotes = false;
    char quote_char = 0;  // initialize to avoid compiler warning
    bool in_space = true;
    // tack on a space so we can deal with end generically
    s += ' ';
    char c;
    string item;
    for (unsigned int i = 0; i < s.size();  ++i) {
        c = s[i];
        if ((c == ' ' || c == '\t') && !in_quotes) {
            if (!in_space) {
                // first space after an item
                result.push_back(item);
                in_space = true;
            }
        } else {
            if (in_space) {
                // first non-space
                item.erase();
                in_space = false;
            }
            if (c == '\'' || c == '"') {
                if (in_quotes) {
                    if (c == quote_char) {
                        // end quote
                        in_quotes = false;
                    } else {
                        // one kind of quote inside of the other
                        item += c;
                    }
                } else {
                    in_quotes = true;
                    quote_char = c;
                }
            } else {
                item += c;
            }
        }
    }
    if (in_quotes) {
        throw runtime_error("Unbalanced quotes (')");
    }
}


static void s_GetTails(const CAlnVec& vec,
                       vector<CContigAssembly::SAlignStats::STails>& tails)
{
    CContigAssembly::SAlignStats::STails tls;
    for (int j = 0;  j < vec.GetNumRows();  ++j) {
        TSeqPos start = vec.GetSeqStart(j);
        TSeqPos stop  = vec.GetSeqStop(j);

        TSeqPos seq_len = vec.GetBioseqHandle(j).GetBioseqLength();

        if (vec.IsPositiveStrand(j)) {
            tls.left = start;
            tls.right = seq_len - stop - 1;
        } else {
            tls.right = start;
            tls.left = seq_len - stop - 1;
        }
        tails.push_back(tls);
    }
}


CRef<CSeq_align_set> CContigAssembly::Blastn(const CSeq_id& query_id,
                                             const CSeq_id& subject_id,
                                             const string& param_string,
                                             CScope& scope)
{
    CSeq_loc query_loc;
    query_loc.SetWhole().Assign(query_id);
    CSeq_loc subject_loc;
    subject_loc.SetWhole().Assign(subject_id);
    return Blastn(query_loc, subject_loc, param_string, scope);
}


CRef<CSeq_align_set> CContigAssembly::Blastn(const CSeq_loc& query_loc,
                                             const CSeq_loc& subject_loc,
                                             const string& param_string,
                                             CScope& scope)
{
    SSeqLoc query_sl(query_loc, scope);
    SSeqLoc subject_sl(subject_loc, scope);

    CBl2Seq bl2seq(query_sl, subject_sl, eBlastn);
    vector<string> args;
    s_SplitCommandLine(param_string, args);
    CBlastNucleotideOptionsHandle& opts =
        dynamic_cast<CBlastNucleotideOptionsHandle&>(bl2seq.SetOptionsHandle());
    opts.SetTraditionalBlastnDefaults();
    for (unsigned int i = 0;  i < args.size();  i += 2) {
        const string& name = args[i];
        if (i + 1 >= args.size()) {
            throw runtime_error("no value given for " + name);
        }
        const string& value = args[i + 1];
        if (name == "-W") {
            opts.SetWordSize(NStr::StringToInt(value));
        } else if (name == "-r") {
            opts.SetMatchReward(NStr::StringToInt(value));
        } else if (name == "-q") {
            opts.SetMismatchPenalty(NStr::StringToInt(value));
        } else if (name == "-e") {
            opts.SetEvalueThreshold(NStr::StringToDouble(value));
        } else if (name == "-Z") {
            opts.SetGapXDropoffFinal(NStr::StringToInt(value));
        } else if (name == "-F") {
            opts.SetFilterString(value.c_str());
        } else if (name == "-G") {
            opts.SetGapOpeningCost(NStr::StringToInt(value));
        } else if (name == "-E") {
            opts.SetGapExtensionCost(NStr::StringToInt(value));
        } else {
            throw runtime_error("invalid option: " + name);
        }
    }

    TSeqAlignVector res = bl2seq.Run();
    return res.front();
}


void CContigAssembly::DiagCounts(const CSeq_align_set& align_set, CScope& scope,
                                 vector<unsigned int>& plus_vec,
                                 vector<unsigned int>& minus_vec)
{
    vector<CConstRef<CDense_seg> > plus_densegs;
    vector<CConstRef<CDense_seg> > minus_densegs;
    ITERATE (CSeq_align_set::Tdata, aln, align_set.Get()) {
        if ((*aln)->GetSeqStrand(0) == eNa_strand_minus) {
            minus_densegs.push_back(CConstRef<CDense_seg>
                                    (&(*aln)->GetSegs().GetDenseg()));
        } else {
            plus_densegs.push_back(CConstRef<CDense_seg>
                                   (&(*aln)->GetSegs().GetDenseg()));
        }
    }

    const CSeq_id& id0 =
        *align_set.Get().front()->GetSegs().GetDenseg().GetIds()[0];
    const CSeq_id& id1 =
        *align_set.Get().front()->GetSegs().GetDenseg().GetIds()[1];

    TSeqPos len0 = scope.GetBioseqHandle(id0).GetBioseqLength();
    TSeqPos len1 = scope.GetBioseqHandle(id1).GetBioseqLength();

    vector<unsigned int> new_plus_vec(len0 + len1);
    swap(plus_vec, new_plus_vec);
    ITERATE (vector<CConstRef<CDense_seg> >, ds, plus_densegs) {
        for (int i = 0; i < (*ds)->GetNumseg(); ++i) {
            TSignedSeqPos start0 = (*ds)->GetStarts()[2 * i];
            TSignedSeqPos start1 = (*ds)->GetStarts()[2 * i + 1];
            if (start0 == -1 || start1 == -1) {
                // do nothing with gaps
                continue;
            }
            TSeqPos seg_len = (*ds)->GetLens()[i];
            TSeqPos diag = start1 - start0 + len0 - 1 ;
            plus_vec[diag] += seg_len;
        }
    }
    vector<unsigned int> new_minus_vec(len0 + len1);
    swap(minus_vec, new_minus_vec);
    ITERATE (vector<CConstRef<CDense_seg> >, ds, minus_densegs) {
        for (int i = 0; i < (*ds)->GetNumseg(); ++i) {
            TSignedSeqPos start0 = (*ds)->GetStarts()[2 * i];
            TSignedSeqPos start1 = (*ds)->GetStarts()[2 * i + 1];
            if (start0 == -1 || start1 == -1) {
                // do nothing with gaps
                continue;
            }
            TSeqPos seg_len = (*ds)->GetLens()[i];
            TSeqPos diag = (start0 + seg_len - 1) + start1;
            minus_vec[diag] += seg_len;
        }
    }
}


void CContigAssembly::FindMaxRange(const vector<unsigned int>& vec,
                                   unsigned int window,
                                   unsigned int& max,
                                   vector<unsigned int>& max_range)
{
    unsigned int running_sum = 0;
    for (unsigned int i = 0; i < window; ++i) {
        running_sum += vec[i];
    }
    max = running_sum;
    max_range.clear();
    max_range.push_back(window - 1);
    for (unsigned int i = window; i < vec.size();  ++i) {
        running_sum = running_sum - vec[i - window] + vec[i];
        if (running_sum >= max) {
            if (running_sum == max) {
                max_range.push_back(i);
            }
            if (running_sum > max) {
                max = running_sum;
                max_range.clear();
                max_range.push_back(i);
            }
        }
    }
}


void CContigAssembly::FindDiagFromAlignSet(const CSeq_align_set& align_set,
                                           CScope& scope,
                                           unsigned int window_size,
                                           ENa_strand& strand,
                                           unsigned int& diag)
{

    vector<unsigned int> plus_vec, minus_vec;
    DiagCounts(align_set, scope, plus_vec, minus_vec);

    unsigned int plus_count;
    vector<unsigned int> plus_range;
    FindMaxRange(plus_vec, window_size, plus_count, plus_range);
    unsigned int minus_count;
    vector<unsigned int> minus_range;
    FindMaxRange(minus_vec, window_size, minus_count, minus_range);
    unsigned int count;
    vector<unsigned int> r;
    if (plus_count > minus_count) {
        strand = eNa_strand_plus;
        count = plus_count;
        r = plus_range;
    } else {
        strand = eNa_strand_minus;
        count = minus_count;
        r = minus_range;
    }

    // check that range is continuous; if not, use first continuous range
    if (r.back() - r.front() + 1 != r.size()) {
        cerr << "Warning: strips with max count not contiguous; "
            "using first contiguous set only" << endl;
        vector<unsigned int> new_r(1, r[0]);
        for (unsigned int i = 1; i < r.size(); ++i) {
            if (r[i] != r[i - 1] + 1) {
                break;
            }
            new_r.push_back(r[i]);
        }
        swap(r, new_r);
    }

    diag = (r.back() + r.front() + 1) / 2 - window_size / 2;
}


// Fake a banded NW alignment around an arbitrary diagonal.
// diag is specified using same convention as returned by
// FindDiagFromAlignSet.
CRef<CDense_seg>
CContigAssembly::BandedGlobalAlignment(const CSeq_id& id0, const CSeq_id& id1,
                                       ENa_strand strand, unsigned int diag,
                                       unsigned int half_width, CScope& scope)
{
    CBioseq_Handle::EVectorStrand vec_strand;
    CBioseq_Handle hand0 = scope.GetBioseqHandle(id0);
    if (strand == eNa_strand_plus) {
        vec_strand = hand0.eStrand_Plus;
    } else {
        vec_strand = hand0.eStrand_Minus;
    }
    CSeqVector vec0 = hand0.GetSeqVector(hand0.eCoding_Iupac, vec_strand);
    vec0.SetIupacCoding();
    string seq0;
    vec0.GetSeqData(0, vec0.size(), seq0);

    CBioseq_Handle hand1 = scope.GetBioseqHandle(id1);
    CSeqVector vec1 = hand1.GetSeqVector(hand1.eCoding_Iupac);
    vec1.SetIupacCoding();
    string seq1;
    vec1.GetSeqData(0, vec1.size(), seq1);

    CBandAligner alnr(seq0, seq1, 0, half_width);
    alnr.SetEndSpaceFree(true, true, true, true);
    // Translate shift from one convention (lower left corner is zero)
    // to another (upper left is zero, direction of shift given separately)
    Uint1 direction;
    size_t shift;
    if (diag <= seq0.size()) {
        direction = 0;
        shift = seq0.size() - 1 - diag;
    } else {
        direction = 1;
        shift = diag - (seq0.size() - 1);
    }
    alnr.SetShift(direction, shift);
    alnr.Run();

    CRef<CDense_seg> ds(new CDense_seg);
    ds->FromTranscript(strand == eNa_strand_plus ? 0 : seq0.size() - 1, strand,
                       0, eNa_strand_plus,
                       alnr.GetTranscriptString());
    CRef<CSeq_id> cr_id0(new CSeq_id);
    cr_id0->Assign(id0);
    CRef<CSeq_id> cr_id1(new CSeq_id);
    cr_id1->Assign(id1);
    ds->SetIds().push_back(cr_id0);
    ds->SetIds().push_back(cr_id1);

    // Trim any overhanging ends
    if (ds->GetStarts().back() == -1
        || ds->GetStarts()[ds->GetStarts().size() - 2] == -1) {
        // remove last segment
        ds->SetStarts().pop_back(); ds->SetStarts().pop_back();
        ds->SetLens().pop_back();
        ds->SetStrands().pop_back(); ds->SetStrands().pop_back();
        ds->SetNumseg(ds->GetNumseg() - 1);
    }
    if (ds->GetStarts()[0] == -1 || ds->GetStarts()[1] == -1) {
        // remove first segment
        for (unsigned int i = 0; i < ds->GetStarts().size() - 2;  ++i) {
            ds->SetStarts()[i] = ds->GetStarts()[i + 2];
        }
        ds->SetStarts().resize(ds->GetStarts().size() - 2);
        for (unsigned int i = 0; i < ds->GetLens().size() - 1;  ++i) {
            ds->SetLens()[i] = ds->GetLens()[i + 1];
        }
        ds->SetLens().resize(ds->GetLens().size() - 1);
        for (unsigned int i = 0; i < ds->GetStrands().size() - 2;  ++i) {
            ds->SetStrands()[i] = ds->GetStrands()[i + 2];
        }
        ds->SetStrands().resize(ds->GetStrands().size() - 2);
        ds->SetNumseg(ds->GetNumseg() - 1);
    }

    return ds;
}


bool CContigAssembly::IsDovetail(const CDense_seg& ds,
                                 unsigned int slop, CScope& scope)
{
    SAlignStats stats;
    CAlnVec avec(ds, scope);
    s_GetTails(avec, stats.tails);

    if( (stats.tails[0].right <= slop && stats.tails[1].left <= slop) ||
        (stats.tails[0].left <= slop && stats.tails[1].right <= slop) ) {
        return true;
    } else {
        return false;
    }
/*
    const CSeq_id& id0 = *ds.GetIds()[0];
    const CSeq_id& id1 = *ds.GetIds()[1];
    TSeqPos len0 = scope.GetBioseqHandle(id0).GetBioseqLength();
    TSeqPos len1 = scope.GetBioseqHandle(id1).GetBioseqLength();

    // This assumes other sequence is plus strand,
    // ie., ds.GetSeqStrand(1) == ncbi.eNa_strand_plus
    if (!ds.CanGetStrands() || ds.GetStrands().empty() ||
         ds.GetSeqStrand(0) == eNa_strand_plus) {
        if (ds.GetSeqStart(0) <= slop &&
            len1 - ds.GetSeqStop(1) - 1 <= slop) {
            return true;
        }
        if (ds.GetSeqStart(1) <= slop &&
            len0 - ds.GetSeqStop(0) - 1 <= slop) {
            return true;
        }
        return false;
    } else {  // seq0 minus strand
        if (ds.GetSeqStart(0) <= slop &&
            ds.GetSeqStart(1) <= slop) {
            return true;
        }
        if (len0 - ds.GetSeqStop(0) - 1 <= slop &&
            len1 - ds.GetSeqStop(1) - 1 <= slop) {
            return true;
        }
        return false;
    }*/
}


// Does the alignment come within slop of either end of
// either sequence?  Perhaps the criteria should be more stringent.
bool CContigAssembly::IsAtLeastHalfDovetail(const CDense_seg& ds,
                                            unsigned int slop, CScope& scope)
{
    const CSeq_id& id0 = *ds.GetIds()[0];
    const CSeq_id& id1 = *ds.GetIds()[1];
    TSeqPos len0 = scope.GetBioseqHandle(id0).GetBioseqLength();
    TSeqPos len1 = scope.GetBioseqHandle(id1).GetBioseqLength();

    return ds.GetSeqStart(0) <= slop
        || len1 - ds.GetSeqStop(1) - 1 <= slop
        || ds.GetSeqStart(1) <= slop
        || len0 - ds.GetSeqStop(0) - 1 <= slop;
}


// Is one contained in the other, modulo slop?
bool CContigAssembly::IsContained(const CDense_seg& ds,
                                  unsigned int slop, CScope& scope)
{
    const CSeq_id& id0 = *ds.GetIds()[0];
    const CSeq_id& id1 = *ds.GetIds()[1];
    TSeqPos len0 = scope.GetBioseqHandle(id0).GetBioseqLength();
    TSeqPos len1 = scope.GetBioseqHandle(id1).GetBioseqLength();
    return (ds.GetSeqStart(0) <= slop
            && len0 - ds.GetSeqStop(0) - 1 <= slop)
            ||
            (ds.GetSeqStart(1) <= slop
             && len1 - ds.GetSeqStop(1) - 1 <= slop);
}


double CContigAssembly::FracIdent(const CDense_seg& ds, CScope& scope)
{
    double Ident = CAlnStats(ds, scope).GetFracIdentity();
    //cerr << "Calc'ed Ident: " << Ident << endl;
    return Ident;
}


// Find the highest-scoring local alignment that is a
// sub-alignment of the given alignment
CRef<CDense_seg> CContigAssembly::BestLocalSubAlignment(const CDense_seg& ds_in,
                                                        CScope& scope)
{
    int Wg = -5, Wm = 1, Wms = -2, Ws = -2;

    CAlnVec avec(ds_in, scope);
    avec.SetEndChar('-');
    avec.SetGapChar('-');

    unsigned int sz = 0;
    for (unsigned int i = 0; i < ds_in.GetLens().size(); ++i) {
        sz += ds_in.GetLens()[i];
    }
    vector<int> scores(sz);
    int previous_score;
    for (unsigned int i = 0; i < scores.size(); ++i) {
        unsigned char res0 = avec.GetResidue(0, i);
        unsigned char res1 = avec.GetResidue(1, i);
        if (i > 0) {
            previous_score = scores[i - 1];
        } else {
            previous_score = 0;
        }
        if (res0 == '-') {
            if (i > 0 && avec.GetResidue(0, i - 1) == '-') {
                scores[i] = previous_score + Ws;
            } else {
                scores[i] = previous_score + Wg + Ws;
            }
        } else if (res1 == '-') {
            if (i > 0 && avec.GetResidue(1, i - 1) == '-') {
                scores[i] = previous_score + Ws;
            } else {
                scores[i] = previous_score + Wg + Ws;
            }
        } else if (res0 == res1) {
            // match
            scores[i] = previous_score + Wm;
        } else {
            // mismatch
            scores[i] = previous_score + Wms;
        }

        // Don't let the score drop below zero
        if (scores[i] < 0) {
            scores[i] = 0;
        }
    }

    // Find the (or a) place where score is max
    int max_score = 0;
    unsigned int right_end = 0;  // initialize to avoid compiler warning
    unsigned int left_end;
    for (unsigned int i = 0; i < scores.size(); ++i) {
        if (scores[i] > max_score) {
            max_score = scores[i];
            right_end = i;  // important that we do ">" rather than ">="
        }
    }

    // Find the closest zero prior to this; the column after this
    // is what we want.  If we never hit zero, we want position zero.
    if (right_end == 0) {
        left_end = 0;  // This will return an alignment of length one.
                       // If score[0] == 0, it is possible that we should
                       // return an alignment of length zero, so this is
                       // not strictly correct.
    } else {
        unsigned int i;
        for (i = right_end - 1; i > 0; --i) {
            if (scores[i] == 0) {
                break;
            }
        }
        if (i > 0) {
            left_end = i + 1;
        } else {
            if (scores[0] == 0) {
                left_end = 1;
            } else {
                left_end = 0;
            }
        }
    }

    // Extract a slice of the ds corresponding to this range
    CRef<CDense_seg> ds_out;
    ds_out = ds_in.ExtractSlice(0, avec.GetSeqPosFromAlnPos(0, left_end),
                                avec.GetSeqPosFromAlnPos(0, right_end));
    return ds_out;
}


vector<CRef<CSeq_align> >
CContigAssembly::Align(const CSeq_id& id0, const CSeq_id& id1,
                       const string& blast_params, double min_ident,
                       unsigned int max_end_slop, CScope& scope,
                       CNcbiOstream* ostr,
                       const vector<unsigned int>& band_halfwidths,
                       unsigned int diag_finding_window,
                       unsigned int min_align_length,
                       ENa_strand strand0, ENa_strand strand1)
{
    if (min_ident > 1 || min_ident < 0) {
        throw runtime_error("min_ident must be between zero and one (got "
                            + NStr::DoubleToString(min_ident) + ")");
    }

    if (ostr) {
        map<int,string> strandmap;
        strandmap[eNa_strand_unknown] = "Unknown";
        strandmap[eNa_strand_plus] = "Plus";
        strandmap[eNa_strand_minus] = "Minus";
        *ostr << "Running blast for " << id0.GetSeqIdString(true)
              << " and " << id1.GetSeqIdString(true) << endl;
        *ostr << "Filtering on " << min_ident << "%, slop " << max_end_slop
              << "bp, min length " << min_align_length << "bp"
              << " and strands " << strandmap[strand0] << ", " << strandmap[strand1] << endl;
    }
    CRef<CSeq_align_set> alns;
    try {
        alns = Blastn(id0, id1, blast_params, scope);
    }
    catch (exception& e) {
        if (ostr) {
            *ostr << "blast failed:\n" << e.what() << endl;
        }
        return vector<CRef<CSeq_align> >();
    }
    cerr << "Blast Count Total: " << alns->Get().size() << endl;
    vector<CRef<CSeq_align> > good_alns;
    NON_CONST_ITERATE (CSeq_align_set::Tdata, aln, alns->Set()) {
        x_OrientAlign((*aln)->SetSegs().SetDenseg(), scope);
double Ident = FracIdent((*aln)->GetSegs().GetDenseg(), scope);
bool Dovetail = IsDovetail((*aln)->GetSegs().GetDenseg(), max_end_slop, scope);
int Len = x_DensegLength((*aln)->GetSegs().GetDenseg());
cerr << "  Ident: " << Ident << "  Dove: " << Dovetail << "  Len: " << Len << endl;
        if (IsDovetail((*aln)->GetSegs().GetDenseg(), max_end_slop, scope)
            && FracIdent((*aln)->GetSegs().GetDenseg(), scope) >= min_ident
            && x_IsAllowedStrands((*aln)->GetSegs().GetDenseg(), strand0, strand1)
            && x_DensegLength((*aln)->GetSegs().GetDenseg()) >= min_align_length ) {
            good_alns.push_back(*aln);
        }
    }
    if (!good_alns.empty()) {
        if (ostr) {
            *ostr << "Found "<< good_alns.size() <<
                " acceptable dovetail alignment(s) by blast "
                  << blast_params << endl;
        }
        return good_alns;
    } else {
        if (ostr) {
            *ostr << "Found no acceptable dovetail alignments by blast "
                  << blast_params << endl;
        }
        if (alns->Get().empty()) {
            if (ostr) {
                *ostr << "No alignments found by blast; "
                    "can't do banded alignment" << endl;
            }
            return vector<CRef<CSeq_align> >();
        }
        ENa_strand strand;
        unsigned int diag;
        FindDiagFromAlignSet(*alns, scope, diag_finding_window, strand, diag);
        CRef<CDense_seg> local_ds;
        ITERATE(vector<unsigned int>, band_halfwidth, band_halfwidths) {
            if (ostr) {
                *ostr << "Trying banded global alignment with bandwidth = "
                      << 2 * *band_halfwidth + 1 << endl;
            }
            CRef<CDense_seg> global_ds;
            try {
                global_ds =
                    BandedGlobalAlignment(id0, id1, strand,
                                          diag, *band_halfwidth, scope);
            }
            catch (CAlgoAlignException& e) {
                if (ostr) {
                    *ostr << "banded alignment failed:\n" << e.what()  << endl;
                }
                continue;
            }

            if (global_ds->GetNumseg() == 0) {
                if (ostr) {
                    *ostr << "banded alignment failed: num segs == 0\n" << endl;
                }
                continue;
            }

            local_ds = BestLocalSubAlignment(*global_ds, scope);
            x_OrientAlign(*local_ds, scope);
            double frac_ident = FracIdent(*local_ds, scope);
            if (ostr) {
                *ostr << "Fraction identity: " << frac_ident << endl;
            }
            if (IsDovetail(*local_ds, max_end_slop, scope)
                && FracIdent(*local_ds, scope) >= min_ident
                && x_IsAllowedStrands(*local_ds, strand0, strand1)
                && x_DensegLength(*local_ds) >= min_align_length ) {
                if (ostr) {
                    *ostr << "Alignment acceptable (full dovetail)" << endl;
                }
                CRef<CSeq_align> aln(new CSeq_align);
                aln->SetSegs().SetDenseg(*local_ds);
                aln->SetType(aln->eType_partial);
                return vector<CRef<CSeq_align> >(1, aln);
            }
        }

        if (ostr) {
            *ostr << "No acceptable alignments from banded alignment algorithm"
                  << endl;
        }
        // Check for any half-dovetails (including contained)
        // in blast results
        good_alns.clear();
        ITERATE (CSeq_align_set::Tdata, aln, alns->Get()) {
            if (IsAtLeastHalfDovetail((*aln)->GetSegs().GetDenseg(),
                                      max_end_slop, scope)
                && FracIdent((*aln)->GetSegs().GetDenseg(), scope) >= min_ident
                && x_IsAllowedStrands((*aln)->GetSegs().GetDenseg(), strand0, strand1)
                && x_DensegLength((*aln)->GetSegs().GetDenseg()) >= min_align_length
                ) {
                good_alns.push_back(*aln);
            }
        }
        if (ostr) {
            *ostr << "Found " <<  good_alns.size() <<
                " acceptable half-dovetail "
                "or contained alignment(s) by blast" << endl;
        }
        if (!good_alns.empty()) {
            return good_alns;
        } else {
            // Check whether banded alignment is an
            // acceptable half-dovetail (including contained)
            if (local_ds
                && IsAtLeastHalfDovetail(*local_ds, max_end_slop, scope)
                && FracIdent(*local_ds, scope) >= min_ident
                && x_IsAllowedStrands(*local_ds, strand0, strand1)
                && x_DensegLength(*local_ds) >= min_align_length) {
                string dovetail_string;
                if (IsContained(*local_ds, max_end_slop, scope)) {
                    dovetail_string = "contained";
                } else {
                    dovetail_string = "half-dovetail";
                }
                if (ostr) {
                    *ostr << "Banded alignment acceptable ("
                          << dovetail_string << ")" << endl;
                }
                CRef<CSeq_align> aln(new CSeq_align);
                aln->SetSegs().SetDenseg(*local_ds);
                aln->SetType(aln->eType_partial);
                return vector<CRef<CSeq_align> >(1, aln);

            } else {
                if (ostr) {
                    *ostr << "Banded alignment not an acceptable "
                        "half-dovetail or contained" << endl;
                }
                return vector<CRef<CSeq_align> >();
            }
        }
    }
}


CContigAssembly::CAlnStats::CAlnStats(const objects::CDense_seg& ds,
                                      objects::CScope& scope)
{
    // Largely stolen from CCOPair::CAln
    string row1, row2;
    CAlnVec vec(ds, scope);
    vec.SetGapChar('-');
    vec.GetAlnSeqString(row1, 0, CAlnMap::TSignedRange
                          (0, vec.GetAlnStop()));
    vec.GetAlnSeqString(row2, 1, CAlnMap::TSignedRange
                          (0, vec.GetAlnStop()));
    _ASSERT(row1.size() == row2.size());

    m_AdjustedLen = m_MM = m_Gaps = 0;
    for (unsigned int i = 0;  i < row1.size();  ++i) {
        if (row1[i] != 'N'  &&  row2[i] != 'N') {
            ++m_AdjustedLen;

            if (row1[i] != row2[i]) {
                if (row1[i] == '-') {
                    ++m_Gaps;
                    while (i+1 < row1.size()  &&  row1[i+1] == '-') ++i;
                } else if (row2[i] == '-') {
                    ++m_Gaps;
                    while (i+1 < row1.size()  &&  row2[i+1] == '-') ++i;
                } else {
                    ++m_MM;
                }
            }
        }
    }
}



void CContigAssembly::GatherAlignStats(const CAlnVec& vec,
                                       SAlignStats& align_stats)
{
    ///
    /// gap metrics
    ///

    align_stats.total_length = 0;
    align_stats.aligned_length = 0;
    align_stats.gap_count = 0;
    align_stats.gaps.clear();
    align_stats.is_simple.clear();

    vector<CRef<CSeq_loc> > dust_locs;

    if (vec.GetNumSegs() > 1) {
        // run dust to classify gaps as simple sequence or not
        CSymDustMasker masker;
        for (int row = 0;  row < vec.GetNumRows();  ++row) {
            CSeqVector seq_vec = vec.GetBioseqHandle(row).GetSeqVector();
            seq_vec.SetIupacCoding();
            CSeq_id id("lcl|dummy");
            CRef<CPacked_seqint> res = masker.GetMaskedInts(id, seq_vec);
            CRef<CSeq_loc> loc(new CSeq_loc);
            loc->SetPacked_int(*res);
            dust_locs.push_back(loc);
        }
    }

    int gap_simple = -1;  // -1 = not checked, 0 = no, 1 = yes
    bool simple = false;
    for (int i = 0;  i < vec.GetNumSegs();  ++i) {
        align_stats.total_length += vec.GetLen(i);
        bool is_gap = false;
        for (int j = 0;  j < vec.GetNumRows();  ++j) {
            if (vec.GetStart(j, i) == -1) {
                simple = false;
                unsigned int other_row = (j + 1) % 2;
                TSeqPos start = vec.GetStart(other_row, i);
                TSeqPos stop = start + vec.GetLen(i);
                string seq;
                vec.GetBioseqHandle(other_row)
                    .GetSeqVector(CBioseq_Handle::eCoding_Iupac,
                                    CBioseq_Handle::eStrand_Plus)
                    .GetSeqData(start, stop, seq);

                CSeq_loc gap_loc;
                gap_loc.SetInt().SetId().Set("lcl|dummy");
                gap_loc.SetInt().SetFrom(vec.GetStart(other_row, i));
                gap_loc.SetInt().SetTo(vec.GetStop(other_row, i));
                sequence::ECompare cmp_res
                    = sequence::Compare(gap_loc, *dust_locs[other_row],
                                        &vec.GetScope());
                simple = cmp_res == sequence::eContained
                    || cmp_res == sequence::eSame;

                if (simple) {
                    gap_simple = 1;
                } else if (gap_simple == -1) {
                    gap_simple = 0;
                }

                is_gap = true;
            }
        }

        if (!is_gap) {
            align_stats.aligned_length += vec.GetLen(i);
        } else {
            align_stats.gap_count += 1;
            align_stats.gaps.push_back(vec.GetLen(i));
            align_stats.is_simple.push_back(simple);
        }
    }

    ///
    /// identity computation
    ///

    unsigned int identities = 0;
    for (int i = 0;  i < vec.GetNumSegs();  ++i) {
        string s1;
        vec.GetSegSeqString(s1, 0, i);
        for (int j = 1;  j < vec.GetNumRows();  ++j) {
            string s2;
            vec.GetSegSeqString(s2, j, i);

            for (unsigned int k = 0;  k < min(s1.size(), s2.size());  ++k) {
                identities += (s1[k] == s2[k]);
            }
        }
    }

    align_stats.mismatches = align_stats.aligned_length - identities;
    align_stats.pct_identity =
        100.0 * double(identities) / double(align_stats.aligned_length);

    ///
    /// overhangs (unaligned tails)
    ///

    s_GetTails(vec, align_stats.tails);

}


void CContigAssembly::GatherAlignStats(const CDense_seg& ds,
                                       CScope& scope,
                                       SAlignStats& align_stats)
{
    CAlnVec avec(ds, scope);
    GatherAlignStats(avec, align_stats);
}


void CContigAssembly::GatherAlignStats(const CSeq_align& aln,
                                       CScope& scope,
                                       SAlignStats& align_stats)
{
    GatherAlignStats(aln.GetSegs().GetDenseg(), scope, align_stats);
}


void CContigAssembly::x_OrientAlign(CDense_seg& ds, CScope& scope)
{
    SAlignStats stats;
    CAlnVec avec(ds, scope);
    s_GetTails(avec, stats.tails);

    if(stats.tails[0].left < stats.tails[1].left) {
        ds.Reverse();
    }
}


bool CContigAssembly::x_IsAllowedStrands(const CDense_seg& ds,
                                       ENa_strand strand0,
                                       ENa_strand strand1)
{
    ENa_strand align_strands[2];
    bool matches[2] = {false, false};
    if(!ds.CanGetStrands() || ds.GetStrands().empty()) {
        align_strands[0] = align_strands[1] = eNa_strand_plus;
    } else {
        align_strands[0] = ds.GetSeqStrand(0);
        align_strands[1] = ds.GetSeqStrand(1);
    }

    if(strand0 == align_strands[0] || strand0 == eNa_strand_unknown)
        matches[0] = true;
    if(strand1 == align_strands[1] || strand1 == eNa_strand_unknown)
        matches[1] = true;

    return (matches[0] & matches[1]);
}


TSeqPos CContigAssembly::x_DensegLength(const objects::CDense_seg& ds)
{
    TSeqPos Length = 0;
    const CDense_seg::TStarts& Starts = ds.GetStarts();
    const CDense_seg::TLens& Lens = ds.GetLens();
    int Dim = ds.GetDim();

    for(unsigned int Seg = 0; Seg < Lens.size(); Seg++) {

        if(Starts[(Seg*Dim)] == -1 || Starts[(Seg*Dim)+1] == -1)
            Length++;
        else
            Length += Lens[Seg];
    }
    return Length;
}


END_NCBI_SCOPE
