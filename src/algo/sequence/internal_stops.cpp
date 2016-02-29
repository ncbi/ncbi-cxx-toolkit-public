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
 * Authors:  Vyacheslav Chetvernin
 *
 * File Description:
 *
 */
#include <ncbi_pch.hpp>

#include <algo/sequence/internal_stops.hpp>
#include "feature_generator.hpp"

#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE

CInternalStopFinder::CInternalStopFinder(CScope& a_scope)
    : scope(a_scope), generator(a_scope)
{
    generator.SetFlags(CFeatureGenerator::fTrimEnds);
    generator.SetAllowedUnaligned(10);
}

typedef map<TSeqRange, bool> TStarts;

pair<set<TSeqPos>, set<TSeqPos> > CInternalStopFinder::FindStartsStops(const CSeq_align& align, int padding)
{
    pair<TStarts, set<TSeqRange> > start_stop_ranges = FindStartStopRanges(align, padding);
    set<TSeqPos> starts, stops;
    ITERATE (TStarts, r, start_stop_ranges.first) {
        starts.insert(r->first.GetFrom());
    }
    ITERATE (set<TSeqRange>, r, start_stop_ranges.second) {
        stops.insert(r->GetFrom());
    }
    return make_pair(starts, stops);
}
pair<TStarts, set<TSeqRange> > CInternalStopFinder::FindStartStopRanges(const CSeq_align& align, int padding,
                                                                        set<TSignedSeqRange>* gaps)
{
    CBioseq_Handle bsh = scope.GetBioseqHandle(align.GetSeq_id(1));
    int genomic_length = bsh.GetBioseqLength();

    CConstRef<CSeq_align> clean_align;
    pair<bool, bool> trim_by_contig(false, false);
    {{
        CConstRef<CSeq_align> padded_align(&align);
        if (padding > 0) {
            CRef<CSeq_loc> loc = align.CreateRowSeq_loc(1);
            int start = loc->GetStart(eExtreme_Positional);
            int stop = loc->GetStop(eExtreme_Positional);

            bool is_circular = (bsh.GetInst_Topology() == CSeq_inst::eTopology_circular);

            if (is_circular) {
                //prevent self overlap
                padding = min(padding, ((stop > start ? genomic_length : 0) - (stop - start +1))/2);
            }
            
            start -= padding;
            stop += padding;

            if (start <= 2 && !is_circular) {
                trim_by_contig.first = true;
            }
            if (stop >= genomic_length-3 && !is_circular) {
                trim_by_contig.second = true;
            }

            if (start < 0) {
                start = is_circular ? start + genomic_length : 0;
            }
            if (stop >= genomic_length) {
                stop = is_circular ? stop - genomic_length : genomic_length-1;
            }
            padded_align = generator.AdjustAlignment(align, TSeqRange(start, stop));
            //cerr << MSerial_AsnText << *padded_align;
        }

        clean_align = generator.CleanAlignment(*padded_align);
        //cerr << MSerial_AsnText << *clean_align;
    }}

    CSeq_loc_Mapper mapper(*clean_align, 1);

    CRef<CSeq_loc> query_loc(new CSeq_loc);
    const CSpliced_seg& spl = clean_align->GetSegs().GetSpliced();
    query_loc->SetInt(*spl.GetExons().front()->CreateRowSeq_interval(0, spl));

    const bool is_protein = (spl.GetProduct_type() == CSpliced_seg::eProduct_type_protein);

    if (!is_protein) {
        NCBI_USER_THROW("CInternalStopFinder::FindStartStopRanges implementation for transcript alignments is sketchy. Will fix when real need appears.");
    }

    string seq = GetCDSNucleotideSequence(*clean_align);
    if (seq.size()%3 != 0) {
        _ASSERT(seq.size()%3 == 0);
        NCBI_USER_THROW("CDSNucleotideSequence not divisible by 3");
    }

    int gcode = 1;
    try {
        gcode = sequence::GetOrg_ref(bsh).GetGcode();
    } catch (CException&) {
    }

    CRef<CGenetic_code::C_E> c_e(new CGenetic_code::C_E); c_e->SetId(gcode);
    CRef<CGenetic_code> code(new CGenetic_code);
    code->Set().push_back(c_e);

    const CTrans_table & tbl = CGen_code_table::GetTransTable(*code);
    const size_t kUnknownState = tbl.SetCodonState('N', 'N', 'N');

    TStarts starts;
    set<TSeqRange> stops;

    size_t state = 0;
    int k = 0;

    ITERATE(string, s, seq) {
        state = tbl.NextCodonState(state, *s);

        if (++k%3)
            continue;

        if (state == kUnknownState)
            continue;

        if (tbl.IsOrfStart(state) || tbl.IsOrfStop(state)) {
            if (is_protein) {
                query_loc->SetInt().SetFrom((k-3)/3);
                query_loc->SetInt().SetTo((k-3)/3);
            } else {
                query_loc->SetInt().SetFrom(k-3);
                query_loc->SetInt().SetTo(k-1);
            }
            TSeqPos mapped_pos = mapper.Map(*query_loc)->GetStart(eExtreme_Biological);
            if (mapped_pos == kInvalidSeqPos)
                continue;
            TSeqPos mapped_pos2 = mapper.Map(*query_loc)->GetStop(eExtreme_Biological);

            if (tbl.IsOrfStart(state)) {
                starts[TSeqRange(mapped_pos, mapped_pos2)] = tbl.IsATGStart(state);
            }
            if (tbl.IsOrfStop(state)) {
                stops.insert(TSeqRange(mapped_pos, mapped_pos2));
            }
        }
    }

    if (gaps != nullptr) {

        CRef<CSeq_loc> region_loc = clean_align->CreateRowSeq_loc(1);
        if (trim_by_contig.first && region_loc->GetStart(eExtreme_Positional) < 3) {
            region_loc = region_loc->Merge(CSeq_loc::fMerge_SingleRange, nullptr);
            region_loc->SetInt().SetFrom(0);
        }
        if (trim_by_contig.second && int(region_loc->GetStop(eExtreme_Positional)) > genomic_length -3) {
            region_loc = region_loc->Merge(CSeq_loc::fMerge_SingleRange, nullptr);
            region_loc->SetInt().SetTo(genomic_length-1);
        }
//         cerr << MSerial_AsnText << *region_loc;

        CSeqVector region_vec(*region_loc, scope,
                               CBioseq_Handle::eCoding_Iupac);
        string region_seq;
        region_vec.GetSeqData(region_vec.begin(), region_vec.end(), region_seq);

        region_seq += 'X'; // to finish last run of Ns

        int gap_begin = -1;
        int gap_end = -1;
        int k = 0;

        CRef<CSeq_id> id(new CSeq_id);
        id->Assign(*region_loc->GetId());
        CRef<CSeq_loc> query_loc(new CSeq_loc(*id, 0, region_vec.size()-1));
        CSeq_loc_Mapper mapper(*query_loc, *region_loc);

        for (auto s: region_seq) {
            if (s == 'N') {
                if (gap_end != k) {
                    gap_begin = k;
                }
                gap_end = k+1;
            } else if (gap_end == k) {
                query_loc->SetInt().SetFrom(gap_begin);
                query_loc->SetInt().SetTo(gap_end-1);
                
                auto mapped_loc = mapper.Map(*query_loc);
                TSeqPos mapped_pos = mapped_loc->GetStart(eExtreme_Biological);
                TSeqPos mapped_pos2 = mapped_loc->GetStop(eExtreme_Biological);
                if (mapped_pos == kInvalidSeqPos || mapped_pos2 == kInvalidSeqPos) {
                    NCBI_USER_THROW("Cannot map Ns run");
                }
                gaps->insert(TSignedSeqRange(mapped_pos, mapped_pos2));
            }
            ++k;
        }

        auto strand = region_loc->GetStrand();

        if (trim_by_contig.first) {
            int gap_stop = -1;
            if (strand != eNa_strand_minus) {
                if (!gaps->empty() && gaps->begin()->GetFrom()==0) {
                    gap_stop = gaps->begin()->GetTo();
                    gaps->erase(gaps->begin());
                }
                gaps->insert(TSignedSeqRange(gap_stop -9, gap_stop));
            } else {
                if (!gaps->empty() && gaps->begin()->GetTo()==0) {
                    gap_stop = gaps->begin()->GetFrom();
                    gaps->erase(gaps->begin());
                }
                gaps->insert(TSignedSeqRange(gap_stop, gap_stop -9));
            }
        }
        if (trim_by_contig.second) {
            int gap_start = genomic_length;
            if (strand != eNa_strand_minus) {
                if (!gaps->empty() && gaps->rbegin()->GetTo()==genomic_length-1) {
                    gap_start = gaps->rbegin()->GetFrom();
                    gaps->erase(prev(gaps->end()));
                }
                gaps->insert(TSignedSeqRange(gap_start, gap_start +9));
            } else {
                if (!gaps->empty() && gaps->rbegin()->GetFrom()==genomic_length-1) {
                    gap_start = gaps->rbegin()->GetTo();
                    gaps->erase(prev(gaps->end()));
                }
                gaps->insert(TSignedSeqRange(gap_start +9, gap_start));
            }
        }
    }

    return make_pair(starts, stops);
}

set<TSeqPos> CInternalStopFinder::FindStops(const CSeq_align& align)
{
    return FindStartsStops(align).second;
}

bool CInternalStopFinder::HasInternalStops(const CSeq_align& align)
{
    return !FindStops(align).empty();
}

string CInternalStopFinder::GetCDSNucleotideSequence(const CSeq_align& align)
{
    if (!align.GetSegs().IsSpliced()) {
        NCBI_THROW(CException, eUnknown, "CInternalStopFinder supports Spliced-seg alignments only");
    }

    string mRNA;

    const CSpliced_seg& spliced_seg = align.GetSegs().GetSpliced();

    int next_prod_start = 0;
    int cds_len = spliced_seg.GetProduct_length();

    if (spliced_seg.GetProduct_type() == CSpliced_seg::eProduct_type_transcript) {
        const CSeq_id& product_id = spliced_seg.GetProduct_id();
        CMappedFeat cds_on_rna = GetCdsOnMrna(product_id, scope);
        if (!cds_on_rna) {
            /// No CDS
            return kEmptyStr;
        }
        if (cds_on_rna.GetLocation().GetStrand() == eNa_strand_minus) {
            NCBI_THROW(CException, eUnknown, "minus strand cdregion on mrna is not supported");
        }
        next_prod_start = cds_on_rna.GetLocation().GetStart(eExtreme_Biological);
        cds_len = cds_on_rna.GetLocation().GetTotalRange().GetLength();

        if (!cds_on_rna.GetLocation().IsPartialStop(eExtreme_Biological)) {
            cds_len -= 3;
        }
    } else {
        cds_len *=3;
    }

    ITERATE( CSpliced_seg::TExons, exon, spliced_seg.GetExons() ) {

        int prod_pos_start = (*exon)->GetProduct_start().AsSeqPos();

        CRef<CSeq_loc> subject_loc(new CSeq_loc);
        subject_loc->SetInt(*(*exon)->CreateRowSeq_interval(1, spliced_seg));
        CSeqVector subject_vec(*subject_loc, scope,
                               CBioseq_Handle::eCoding_Iupac);
        string subject_seq;
        subject_vec.GetSeqData(subject_vec.begin(), subject_vec.end(), subject_seq);
        int subj_pos = 0;

        if (next_prod_start < prod_pos_start) {
            mRNA.append(prod_pos_start - next_prod_start, 'N');
            next_prod_start = prod_pos_start;
        }

        if ((*exon)->IsSetParts()) {
            ITERATE (CSpliced_exon::TParts, part_it, (*exon)->GetParts()) {
                pair<int, int> chunk = ChunkSize(**part_it);
                prod_pos_start += chunk.second;
                if (chunk.first == 0) {
                    if (next_prod_start < prod_pos_start) {
                        mRNA.append(prod_pos_start - next_prod_start, 'N');
                        next_prod_start = prod_pos_start;
                    }
                } else if (chunk.second > 0) {
                    if (next_prod_start < prod_pos_start) {
                        mRNA.append(subject_seq, subj_pos+chunk.second-(prod_pos_start - next_prod_start), prod_pos_start - next_prod_start);
                        next_prod_start = prod_pos_start;
                    }
                }
                subj_pos += chunk.first;
            }
        } else {
            mRNA.append(subject_seq);
            next_prod_start += subject_seq.size();
        }
    }

    return mRNA.substr(0, cds_len);
}


// pair(genomic, product)
pair<int, int> ChunkSize(const CSpliced_exon_chunk& chunk)
{
    int len = 0;
    switch (chunk.Which()) {
    case CSpliced_exon_chunk::e_Genomic_ins:
        len = chunk.GetGenomic_ins();
        return make_pair(len, 0);
    case CSpliced_exon_chunk::e_Product_ins:
        len = chunk.GetProduct_ins();
        return make_pair(0, len);
    case CSpliced_exon_chunk::e_Match:
        len = chunk.GetMatch();
        break;
    case CSpliced_exon_chunk::e_Mismatch:
        len = chunk.GetMismatch();
        break;
    case CSpliced_exon_chunk::e_Diag:
        len = chunk.GetDiag();
        break;
    default:
        NCBI_THROW(CException, eUnknown, "Spliced_exon_chunk type not set");
    }
    return make_pair(len, len);
}

END_NCBI_SCOPE
