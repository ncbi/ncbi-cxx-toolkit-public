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
#include <algo/align/nw/nw_band_aligner.hpp>
#include <objtools/alnmgr/alnvec.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(blast);

// Split a blast parameter string such as
// "-W 28 -r 1 -q -3 -e 1e-5 -Z 200 -F 'm L; R -d rodents.lib'"
// into ["-W", "28",..."-F", "m L; R -d rodents.lib"]
// (i.e., respect single quotes)
static void s_SplitCommandLine(string s, vector<string>& result) {
    bool in_quotes = false;
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
                item.clear();
                in_space = false;
            }
            if (c == '\'') {
                in_quotes = !in_quotes;
            } else {
                item += c;
            }
        }
    }
    if (in_quotes) {
        throw runtime_error("Unbalanced quotes (')");
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
    CRef<CSeq_align> aln = res.front()->Set().front();
    return CRef<CSeq_align_set>(&aln->SetSegs().SetDisc());
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
        for (unsigned int i = 0; i < r.size(); ++i) {
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

    // adjust diag and half_width to fake the non-main diagonal thing
    if (diag > vec0.size()) {
        diag -= half_width;
    } else {
        diag += half_width;
    }
    half_width *= 2;

    unsigned int largest_diag = vec0.size() + vec1.size() - 2;
    TSeqPos begin0, end0, begin1, end1;
    string sub_seq0, sub_seq1;
    if (strand == eNa_strand_plus) {
        begin0 = max(0, int(vec0.size()) - int(diag) - 1);
        end0   = min(int(vec0.size()) - 1, int(largest_diag) - int(diag));
        begin1 = max(0, int(vec1.size()) - (int(largest_diag) - int(diag)) - 1);
        end1 = min(int(vec1.size()) - 1, int(diag));
        sub_seq0 = seq0.substr(begin0, end0 - begin0 + 1);
        sub_seq1 = seq1.substr(begin1, end1 - begin1 + 1);
    } else {
        begin0 = max(0, int(diag) - int(vec1.size()) + 1);
        end0 = min(vec0.size() - 1, diag);
        begin1 = max(0, int(diag) - int(vec0.size()) + 1);
        end1 = min(int(vec1.size()) - 1, int(diag));
        sub_seq0 = seq0.substr(vec0.size() - end0 - 1, end0 - begin0 + 1);
        sub_seq1 = seq1.substr(begin1, end1 - begin1 + 1);
    }

    CBandAligner alnr(sub_seq0, sub_seq1, 0, half_width);
    alnr.SetEndSpaceFree(true, true, true, true);

    // due to nasty trick for non-main diagonal
    if (diag > vec0.size()) {
        alnr.SetEndSpaceFree(true, false, false, true);
    } else {
        alnr.SetEndSpaceFree(false, true, true, false);
    }
    alnr.Run();

    TSeqPos pos0;
    CRef<CDense_seg> ds(new CDense_seg);
    if (strand == eNa_strand_plus) {
        pos0 = begin0;
    } else {
        pos0 = end0;
    }
    ds->FromTranscript(pos0, strand,
                       begin1, eNa_strand_plus,
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
    const CSeq_id& id0 = *ds.GetIds()[0];
    const CSeq_id& id1 = *ds.GetIds()[1];
    TSeqPos len0 = scope.GetBioseqHandle(id0).GetBioseqLength();
    TSeqPos len1 = scope.GetBioseqHandle(id1).GetBioseqLength();

    // This assumes other sequence is plus strand,
    // ie., ds.GetSeqStrand(1) == ncbi.eNa_strand_plus
    if (ds.GetSeqStrand(0) == eNa_strand_plus) {
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
    }
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
    CAlnVec avec(ds, scope);
    avec.SetGapChar('-');
    avec.SetEndChar('-');
    string row0, row1;
    avec.GetAlnSeqString(row0, 0, CRange<TSignedSeqPos>(0, avec.GetAlnStop()));
    avec.GetAlnSeqString(row1, 1, CRange<TSignedSeqPos>(0, avec.GetAlnStop()));
    TSeqPos total_len = 0;
    for (unsigned int i = 0; i < ds.GetLens().size(); ++i) {
        total_len += ds.GetLens()[i];
    }
    TSeqPos ident_count = 0;
    for (unsigned int i = 0; i < total_len; ++i) {
        if (row0[i] == row1[i]) {
            // This relies on there never being gaps
            // in both sequences in the same column
            ident_count += 1;
        }
    }
    return double(ident_count) / total_len;
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
            if (i > 0 and avec.GetResidue(0, i - 1) == '-') {
                scores[i] = previous_score + Ws;
            } else {
                scores[i] = previous_score + Wg + Ws;
            }
        } else if (res1 == '-') {
            if (i > 0 and avec.GetResidue(1, i - 1) == '-') {
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
    unsigned int right_end, left_end;
    for (unsigned int i = 0; i < scores.size(); ++i) {
        if (scores[i] > max_score) {
            max_score = scores[i];
            right_end = i;  // important that we do ">" rather than ">="
        }
    }

    // Find the closest zero prior to this; the column after this
    // is what we want.  If we never hit zero, we want position zero.
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
                       CNcbiOstream* ostr, unsigned int band_halfwidth,
                       unsigned int diag_finding_window)
{
    if (min_ident > 1 || min_ident < 0) {
        throw runtime_error("min_ident must be between zero and one (got "
                            + NStr::DoubleToString(min_ident) + ")");
    }

    if (ostr) {
        *ostr << "Running blast for " << id0.GetSeqIdString()
              << " and " << id1.GetSeqIdString() << endl;
    }
    CRef<CSeq_align_set> alns = Blastn(id0, id1, blast_params, scope);
    vector<CRef<CSeq_align> > good_alns;
    ITERATE (CSeq_align_set::Tdata, aln, alns->Get()) {
        if (IsDovetail((*aln)->GetSegs().GetDenseg(), max_end_slop, scope)
            && FracIdent((*aln)->GetSegs().GetDenseg(), scope) >= min_ident) {
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
            *ostr << "Trying banded global alignment" << endl;
        }
        ENa_strand strand;
        unsigned int diag;
        FindDiagFromAlignSet(*alns, scope, diag_finding_window, strand, diag);
        CRef<CDense_seg> global_ds =
            BandedGlobalAlignment(id0, id1, strand,
                                  diag, band_halfwidth, scope);
        CRef<CDense_seg> local_ds = BestLocalSubAlignment(*global_ds, scope);
        double frac_ident = FracIdent(*local_ds, scope);
        if (ostr) {
            *ostr << "Fraction identity: " << frac_ident << endl;
        }
        if (IsAtLeastHalfDovetail(*local_ds, max_end_slop, scope)
            && FracIdent(*local_ds, scope) >= min_ident) {
            string dovetail_string;
            if (IsDovetail(*local_ds, max_end_slop, scope)) {
                dovetail_string = "full dovetail";
            } else if (IsContained(*local_ds, max_end_slop, scope)) {
                dovetail_string = "contained";
            } else {
                dovetail_string = "half-dovetail";
            }
            if (ostr) {
                *ostr << "Alignment acceptable (" << dovetail_string
                      << ")" << endl;
            }
            CRef<CSeq_align> aln(new CSeq_align);
            aln->SetSegs().SetDenseg(*local_ds);
            aln->SetType(aln->eType_partial);
            return vector<CRef<CSeq_align> >(1, aln);
         } else {
             if (ostr) {
                 *ostr << "Alignment not acceptable" << endl;
             }
             // Check for any half-dovetails (including contained)
             // in blast results
             good_alns.clear();
             ITERATE (CSeq_align_set::Tdata, aln, alns->Get()) {
                 if (IsAtLeastHalfDovetail((*aln)->GetSegs().GetDenseg(),
                                           max_end_slop, scope)
                     && FracIdent((*aln)->GetSegs().GetDenseg(), scope)
                     >= min_ident) {
                     good_alns.push_back(*aln);
                 }
             }
             if (ostr) {
                 *ostr << "Found " <<  good_alns.size() <<
                     " acceptable half-dovetail "
                     "or contained alignment(s) by blast" << endl;
             }
             return good_alns;
         }
    }
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/06/16 17:30:23  jcherry
 * Initial version
 *
 * ===========================================================================
 */
