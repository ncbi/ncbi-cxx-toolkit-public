/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software / database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software / database is freely available
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
 * Authors:  Alexandre Souvorov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include "gnomon_seq.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

const EResidue k_toMinus[5] = { enT, enG, enC, enA, enN };
const char *const k_aa_table = "KNKNXTTTTTRSRSXIIMIXXXXXXQHQHXPPPPPRRRRRLLLLLXXXXXEDEDXAAAAAGGGGGVVVVVXXXXX*Y*YXSSSSS*CWCXLFLFXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

void Convert(const CResidueVec& src, CEResidueVec& dst)
{
    int len = src.size();
    dst.clear();
    dst.reserve(len);
    for(int i = 0; i < len; ++i)
	dst.push_back( fromACGT(src[i]) );
}

void Convert(const CResidueVec& src, CDoubleStrandSeq& dst)
{
    Convert(src,dst[ePlus]);
    ReverseComplement(dst[ePlus],dst[eMinus]);
}

void Convert(const CEResidueVec& src, CResidueVec& dst)
{
    int len = src.size();
    dst.clear();
    dst.reserve(len);
    for(int i = 0; i < len; ++i)
	dst.push_back( toACGT(src[i]) );
}

void ReverseComplement(const CEResidueVec& src, CEResidueVec& dst)
{
    int len = src.size();
    dst.clear();
    dst.reserve(len);
    for(int i = len-1; i >= 0; --i)
	dst.push_back(k_toMinus[(int)src[i]]);
}

const TResidue codons[4][4] = {"ATG", "TAA", "TAG", "TGA"};
const TResidue rev_codons[4][4] = {"CAT", "TTA", "CTA", "TCA"};
static const EResidue s_ecodons0 [3] = { enA, enT, enG };
static const EResidue s_ecodons1 [3] = { enT, enA, enA };
static const EResidue s_ecodons2 [3] = { enT, enA, enG };
static const EResidue s_ecodons3 [3] = { enT, enG, enA };
static const EResidue s_ecodons0r[3] = { enC, enA, enT };
static const EResidue s_ecodons1r[3] = { enT, enT, enA };
static const EResidue s_ecodons2r[3] = { enC, enT, enA };
static const EResidue s_ecodons3r[3] = { enT, enC, enA };
const EResidue *ecodons[4] = {s_ecodons0, s_ecodons1, s_ecodons2, s_ecodons3};
const EResidue *rev_ecodons[4] = {s_ecodons0r, s_ecodons1r, s_ecodons2r, s_ecodons3r};

template <typename Res>
class res_traits {
public:
    static Res _fromACGT(TResidue x)
    { return x; }
    static const Res* _codons(int i) { return codons[i]; }
    static const Res* _rev_codons(int i) { return rev_codons[i]; }
};

template<>
class res_traits<EResidue> {
public:
    static EResidue _fromACGT(TResidue x)
    { return fromACGT(x); }
    static const EResidue* _codons(int i) { return ecodons[i]; }
    static const EResidue* _rev_codons(int i) { return rev_ecodons[i]; }
};

template <class Res>
bool IsStartCodon(const Res * seq, int strand)   // seq points to A for both strands
{
    const Res * start_codon;
    if(strand == ePlus)
        start_codon = res_traits<Res>::_codons(0);
    else {
        start_codon = res_traits<Res>::_rev_codons(0);
        seq -= 2;
    }
    return equal(start_codon,start_codon+3,seq);
}

template bool IsStartCodon<EResidue>(const EResidue * seq, int strand);
template bool IsStartCodon<TResidue>(const TResidue * seq, int strand);

template <class Res>
bool IsStopCodon(const Res * seq, int strand)   // seq points to T for both strands
{
    if(strand == ePlus) {
        if (*seq != res_traits<Res>::_codons(1)[0]) // T
            return false;
        ++seq;
        for (int i = 1; i <= 3; ++i) 
            if(equal(res_traits<Res>::_codons(i)+1,res_traits<Res>::_codons(i)+3,seq))
                return true;
        return false;
    } else {
        if (*seq != res_traits<Res>::_rev_codons(1)[2]) // A
            return false;
        seq -= 2;
        for (int i = 1; i <= 3; ++i) 
            if(equal(res_traits<Res>::_rev_codons(i)+1,res_traits<Res>::_rev_codons(i)+3,seq))
                return true;
        return false;
    }
}
template bool IsStopCodon<EResidue>(const EResidue * seq, int strand);

/*
bool CodonWithoutDeletions( const TFrameShifts& fshifts, size_t codon_start, const CFrameShiftedSeqMap& cdsmap)
{
    TSignedSeqPos start = cdsmap.MapEditedToOrig(codon_start);
    TSignedSeqPos stop = cdsmap.MapEditedToOrig(codon_start+2);
    _ASSERT(stop >= 0);
    if (start > stop)
        swap(start,stop);
    _ASSERT( 0<=start );

    ITERATE(TFrameShifts, fsi, fshifts)
        if( fsi->IntersectingWith(start,stop) && (fsi->IsDeletion() || fsi->ReplacementLoc() != -1))
            return false;

    return true;
}
*/

template <class Vec>
bool IsProperStart(size_t pos, const Vec& mrna, const CFrameShiftedSeqMap& mrnamap)
{
    return IsStartCodon(&mrna[pos]) &&
        !mrnamap.EditedRangeIncludesTransformedDeletion(TSignedSeqRange(pos,pos+2))
        ;
}
template bool IsProperStart<CEResidueVec>(size_t pos, const CEResidueVec& mrna, const CFrameShiftedSeqMap& mrnamap);
template bool IsProperStart<CResidueVec>(size_t pos, const CResidueVec& mrna, const CFrameShiftedSeqMap& mrnamap);

template <class Vec>
bool IsProperStop(size_t pos, const Vec& mrna, const CFrameShiftedSeqMap& mrnamap)
{
    return IsStopCodon(&mrna[pos]) && 
        !mrnamap.EditedRangeIncludesTransformedDeletion(TSignedSeqRange(pos,pos+2))
        ;
}
template bool IsProperStop<CResidueVec>(size_t pos, const CResidueVec& mrna, const CFrameShiftedSeqMap& mrnamap);
template bool IsProperStop<CEResidueVec>(size_t pos, const CEResidueVec& mrna, const CFrameShiftedSeqMap& mrnamap);

void FindAllCodonInstances(TIVec positions[], const EResidue codon[], const CEResidueVec& mrna, const CFrameShiftedSeqMap& mrnamap, TSignedSeqRange search_region, int fixed_frame)
{
    for (CEResidueVec::const_iterator pos = mrna.begin()+search_region.GetFrom(); (pos = search(pos,mrna.end(),codon,codon+3)) < mrna.begin()+search_region.GetTo(); ++pos) {
        int l = pos-mrna.begin();
        int frame = l%3;
        if ((fixed_frame==-1 || fixed_frame==frame) && !mrnamap.EditedRangeIncludesTransformedDeletion(TSignedSeqRange(l,l+2)))
            positions[frame].push_back(l);
    }
}

void FindAllStarts(TIVec starts[], const CEResidueVec& mrna, const CFrameShiftedSeqMap& mrnamap, TSignedSeqRange search_region, int fixed_frame)
{
    FindAllCodonInstances(starts, ecodons[0], mrna, mrnamap, search_region, fixed_frame);
}

void FindAllStops(TIVec stops[], const CEResidueVec& mrna, const CFrameShiftedSeqMap& mrnamap, TSignedSeqRange search_region, int fixed_frame)
{
    for (int i=1; i <=3; ++i)
        FindAllCodonInstances(stops, ecodons[i], mrna, mrnamap, search_region, fixed_frame);
    for (int f = 0; f < 3; ++f)
        sort(stops[f].begin(), stops[f].end());
}

namespace {
    EResidue ag[2] = { enA, enG };
 	
bool UpstreamStartOrSplice(const CEResidueVec& seq_strand, int start, int frame) {
    int cds_start = start+frame;
    for(int k = cds_start-2; k >= 0; k -= 1) {
        if(k < start-1 && equal(ag,ag+2,&seq_strand[k]))
            return true;   // splice must be outside of alignment
        if((cds_start-k)%3 != 0)
            continue;
        if(IsStartCodon(&seq_strand[k]))
            return true;
        if(IsStopCodon(&seq_strand[k]))
            return false;
    }
 	
    return false;
}

}

void FindStartsStops(const CGeneModel& model, const CEResidueVec& contig_seq, const CEResidueVec& mrna, const CFrameShiftedSeqMap& mrnamap, TIVec starts[3],  TIVec stops[3], int& frame)
{
    int left_cds_limit = -1;
    int reading_frame_start = mrna.size();
    int reading_frame_stop = mrna.size();
    int right_cds_limit = mrna.size();
    frame = -1;
    EStrand strand = model.Strand();

    if (!model.ReadingFrame().Empty()) {
        left_cds_limit = mrnamap.MapOrigToEdited(model.GetCdsInfo().MaxCdsLimits().GetFrom());
        _ASSERT(left_cds_limit >= 0 || model.GetCdsInfo().MaxCdsLimits().GetFrom() == TSignedSeqRange::GetWholeFrom());
        right_cds_limit = mrnamap.MapOrigToEdited(model.GetCdsInfo().MaxCdsLimits().GetTo());
        _ASSERT(right_cds_limit >= 0 || model.GetCdsInfo().MaxCdsLimits().GetTo() == TSignedSeqRange::GetWholeTo());

        reading_frame_start = mrnamap.MapOrigToEdited(model.ReadingFrame().GetFrom());
        _ASSERT(reading_frame_start >= 0);
        reading_frame_stop = mrnamap.MapOrigToEdited(model.ReadingFrame().GetTo());
        _ASSERT(reading_frame_stop >= 0);

        if(strand == eMinus) {
            std::swap(left_cds_limit,right_cds_limit);
            std::swap(reading_frame_start,reading_frame_stop);
        }
        if(right_cds_limit < 0) right_cds_limit = mrna.size();

        if (reading_frame_start == 0 && IsProperStart(reading_frame_start,mrna,mrnamap))
            reading_frame_start += 3;

        _ASSERT( -1 <= left_cds_limit && left_cds_limit <= reading_frame_start );
        _ASSERT( 0 <= reading_frame_start && reading_frame_start <= reading_frame_stop && reading_frame_stop < int(mrna.size()) );
        _ASSERT( reading_frame_stop <= right_cds_limit && right_cds_limit <= int(mrna.size()) );

        frame = reading_frame_start%3;

        if (left_cds_limit<0) {
            if (reading_frame_start >= 3) {
                FindAllStops(stops,mrna,mrnamap,TSignedSeqRange(0,reading_frame_start),frame);
            }

            if (stops[frame].size()>0)
                left_cds_limit = stops[frame].back()+3;
        } else {
            FindAllStops(stops,mrna,mrnamap,TSignedSeqRange(0,left_cds_limit),frame);
        }
    }

    if (left_cds_limit<0) {
        int model_start = mrnamap.MapEditedToOrig(0);

        if(Include(model.GetCdsInfo().ProtReadingFrame(),model_start)) {
            starts[0].push_back(-3);            // proteins are scored no matter what
        } else {
            if(strand == eMinus)
                model_start = contig_seq.size()-1-model_start; 
            for (int i = 0; i<3; ++i) {
                if (frame == -1 || frame == i) {
                    if (UpstreamStartOrSplice(contig_seq,model_start,i))
                        starts[i].push_back(i-3);
                    else 
                        stops[i].push_back(i-3); // bogus stop to limit MaxCds      
                }
            }
        }

        left_cds_limit = 0;
    }

    FindAllStarts(starts,mrna,mrnamap,TSignedSeqRange(left_cds_limit,reading_frame_start-1),frame);

    if (frame==-1) {
        FindAllStops(stops,mrna,mrnamap,TSignedSeqRange(0,mrna.size()-1),frame);
    } else if (right_cds_limit - reading_frame_stop >= 3) {
        FindAllStops(stops,mrna,mrnamap,TSignedSeqRange(reading_frame_stop+1,right_cds_limit),frame);
    }

    if (int(mrna.size()) <= right_cds_limit) {
        stops[mrna.size()%3].push_back(mrna.size());
        stops[(mrna.size()-1)%3].push_back(mrna.size()-1);
        stops[(mrna.size()-2)%3].push_back(mrna.size()-2);
    }

}

bool FindUpstreamStop(const vector<int>& stops, int start, int& stop)
{
    vector<int>::const_iterator it_stop = lower_bound(stops.begin(),stops.end(),start);
    
    if(it_stop != stops.begin()) {
        stop = *(--it_stop);
        return true;
    } else 
        return false;
}


void CFrameShiftedSeqMap::InsertOneToOneRange(TSignedSeqPos orig_start, TSignedSeqPos edited_start, int len, const CFrameShiftInfo* left_fs, const CFrameShiftInfo* right_fs)
{
    _ASSERT(len > 0);
    _ASSERT(m_orig_ranges.empty() || (orig_start > m_orig_ranges.back().GetTo() && edited_start > m_edited_ranges.back().GetTo()));

    CFrameShiftedSeqMap::SMapRangeEdge orig_from(orig_start);
    if(left_fs != 0 && left_fs->IsInsertion()) {
        orig_from.m_extra = left_fs->Len();
        orig_from.m_was_deletion = (left_fs->ReplacementLoc() >= 0);
    }
    CFrameShiftedSeqMap::SMapRangeEdge orig_to(orig_start+len-1);
    if(right_fs != 0 && right_fs->IsInsertion()) {
        orig_to.m_extra = right_fs->Len();
        orig_to.m_was_deletion = (right_fs->ReplacementLoc() >= 0);
    }
    m_orig_ranges.push_back(SMapRange(orig_from, orig_to));

    CFrameShiftedSeqMap::SMapRangeEdge edited_from(edited_start);
    if(left_fs != 0 && left_fs->IsDeletion()) {
        edited_from.m_extra = left_fs->Len();
    }
    CFrameShiftedSeqMap::SMapRangeEdge edited_to(edited_start+len-1);
    if(right_fs != 0 && right_fs->IsDeletion()) {
        edited_to.m_extra = right_fs->Len();
    }
    m_edited_ranges.push_back(SMapRange(edited_from, edited_to));
}


TSignedSeqPos CFrameShiftedSeqMap::InsertIndelRangesForInterval(TSignedSeqPos orig_a, TSignedSeqPos orig_b, TSignedSeqPos edit_a, TFrameShifts::const_iterator fsi_begin, TFrameShifts::const_iterator fsi_end) 
{
    const CFrameShiftInfo* left_fs = 0;
    const CFrameShiftInfo* right_fs = 0;
    for (TFrameShifts::const_iterator fsi = fsi_begin; fsi != fsi_end; ++fsi) {
        TSignedSeqPos blim = fsi->IsInsertion() ? orig_b : orig_b+1;
        if(fsi->Loc() < orig_a) {
            _ASSERT( !fsi->IntersectingWith(orig_a,orig_b) );
            continue;
        } else if(fsi->Loc() > blim) {
            break;
        } else {
            right_fs = &(*fsi);
            int len = fsi->Loc()-orig_a;
            _ASSERT(left_fs == 0 || len > 0);                               // no two insertion in a row;
            if(len > 0) {
                InsertOneToOneRange(orig_a, edit_a, len, left_fs, right_fs);
                orig_a += len;
                edit_a += len;
            }
            if (fsi->IsInsertion()) {
                orig_a += fsi->Len();
            } else {
                edit_a += fsi->Len();
            }
            left_fs = right_fs;
        }
    }
    if(orig_a <= orig_b) {
        int len = orig_b-orig_a+1;
        InsertOneToOneRange(orig_a, edit_a, len, left_fs, 0);
        edit_a += len;
    }

    return edit_a;
}

CFrameShiftedSeqMap::CFrameShiftedSeqMap(const CGeneModel& align, EMapLimit limit)
{
    m_strand = align.Strand();
    TFrameShifts::const_iterator fsi_begin = align.FrameShifts().begin();
    TFrameShifts::const_iterator fsi_end = align.FrameShifts().end();
    TSignedSeqRange lim = align.Limits();
    if(limit != eNoLimit) {
        lim = (limit == eCdsOnly) ? align.GetCdsInfo().Cds() : align.RealCdsLimits();
        while(fsi_begin != align.FrameShifts().end() && fsi_begin->Loc() <= lim.GetFrom()) {
            _ASSERT(!fsi_begin->IntersectingWith(lim.GetFrom(),lim.GetTo()));
            ++fsi_begin;
        }
        fsi_end = fsi_begin;
        while(fsi_end != align.FrameShifts().end() && fsi_end->Loc() <= lim.GetTo()) ++fsi_end;
        _ASSERT(fsi_end == align.FrameShifts().end() || !fsi_end->IntersectingWith(lim.GetFrom(),lim.GetTo()));
    }

    m_orig_ranges.reserve(align.Exons().size()+fsi_end-fsi_begin);
    m_edited_ranges.reserve(align.Exons().size()+fsi_end-fsi_begin);
    
    TSignedSeqPos estart = 0;
    ITERATE(CGeneModel::TExons, i, align.Exons()) {
        TSignedSeqPos start = i->GetFrom();
        TSignedSeqPos stop = i->GetTo();
        
        if(stop < lim.GetFrom()) continue;
        if(lim.GetTo() < start) break;
        
        start = max(start,lim.GetFrom());
        stop = min(stop,lim.GetTo());

        estart = InsertIndelRangesForInterval(start, stop, estart, fsi_begin, fsi_end);
    }
}

template <class Vec>
void CFrameShiftedSeqMap::EditedSequence(const Vec& original_sequence, Vec& edited_sequence) const
{
    edited_sequence.clear();
    edited_sequence.insert(edited_sequence.end(),m_edited_ranges.front().GetExtraFrom(),res_traits<typename Vec::value_type>::_fromACGT('N'));    
    for(int range = 0; range < (int)m_orig_ranges.size(); ++range) {
        int a = m_orig_ranges[range].GetFrom();
        int b = m_orig_ranges[range].GetTo()+1;
        edited_sequence.insert(edited_sequence.end(),original_sequence.begin()+a, original_sequence.begin()+b);
        edited_sequence.insert(edited_sequence.end(),m_edited_ranges[range].GetExtraTo(),res_traits<typename Vec::value_type>::_fromACGT('N'));

        if(range < (int)m_orig_ranges.size()-1 && m_edited_ranges[range].GetExtendedTo() < m_edited_ranges[range+1].GetExtendedFrom()) {
            edited_sequence.insert(edited_sequence.end(),m_edited_ranges[range+1].GetExtraFrom(),res_traits<typename Vec::value_type>::_fromACGT('N'));
        }
    }
    
    if(m_strand == eMinus) 
        ReverseComplement(edited_sequence.begin(), edited_sequence.end());
}

int CFrameShiftedSeqMap::FindLowerRange(const vector<CFrameShiftedSeqMap::SMapRange>& a,  TSignedSeqPos p) {
    int num = lower_bound(a.begin(), a.end(), CFrameShiftedSeqMap::SMapRange(p+1,p+1))-a.begin()-1;
    return num;
}

template
void CFrameShiftedSeqMap::EditedSequence<CResidueVec>(const CResidueVec& original_sequence, CResidueVec& edited_sequence) const;
template
void CFrameShiftedSeqMap::EditedSequence<CEResidueVec>(const CEResidueVec& original_sequence, CEResidueVec& edited_sequence) const;

bool CFrameShiftedSeqMap::EditedRangeIncludesTransformedDeletion(TSignedSeqRange edited_range) const {

    _ASSERT(edited_range.NotEmpty() && Include(TSignedSeqRange(m_edited_ranges.front().GetFrom(),m_edited_ranges.back().GetTo()), edited_range));

    TSignedSeqPos a = edited_range.GetFrom();
    int numa = FindLowerRange(m_edited_ranges, a);
    _ASSERT(Include(TSignedSeqRange(m_edited_ranges[numa].GetFrom(),m_edited_ranges[numa].GetTo()), a));

    TSignedSeqPos b = edited_range.GetTo();
    int numb = lower_bound(m_edited_ranges.begin(), m_edited_ranges.end(), CFrameShiftedSeqMap::SMapRange(b+1,b+1))-m_edited_ranges.begin()-1;
    _ASSERT(Include(TSignedSeqRange(m_edited_ranges[numb].GetFrom(),m_edited_ranges[numb].GetTo()), b));

    if(m_strand == eMinus) swap(numa, numb);

    for(int i = numa; i < numb; ++i) {
        if(m_orig_ranges[i].RightEndIsTransformedDeletion() || m_orig_ranges[i+1].LeftEndIsTransformedDeletion()) return true;
    }

    return false;
}

TSignedSeqRange CFrameShiftedSeqMap::ShrinkToRealPoints(TSignedSeqRange orig_range) const {

    _ASSERT(orig_range.NotEmpty() && Include(TSignedSeqRange(m_orig_ranges.front().GetExtendedFrom(),m_orig_ranges.back().GetExtendedTo()), orig_range));

    TSignedSeqPos a = orig_range.GetFrom();
    int numa =  FindLowerRange(m_orig_ranges, a);
    if(a > m_orig_ranges[numa].GetTo()) {
        a = m_orig_ranges[numa+1].GetFrom();
    }

    TSignedSeqPos b = orig_range.GetTo();
    int numb =  FindLowerRange(m_orig_ranges, b);
    if(b > m_orig_ranges[numb].GetTo()) {
        b = m_orig_ranges[numb].GetTo();
    }

    return TSignedSeqRange(a, b);
}

TSignedSeqPos CFrameShiftedSeqMap::FShiftedMove(TSignedSeqPos orig_pos, int len) const {
    orig_pos = MapOrigToEdited(orig_pos);
    _ASSERT(orig_pos >= 0);
    if(m_strand == ePlus)
        orig_pos += len;
    else
        orig_pos -= len;
    orig_pos = MapEditedToOrig(orig_pos);
    _ASSERT(orig_pos >= 0);
    return orig_pos;
}


TSignedSeqPos CFrameShiftedSeqMap::MapAtoB(const vector<CFrameShiftedSeqMap::SMapRange>& a, const vector<CFrameShiftedSeqMap::SMapRange>& b, TSignedSeqPos p, ERangeEnd move_mode) {
    if(p < a.front().GetExtendedFrom() || p > a.back().GetExtendedTo()) return -1;

    if(p < a.front().GetFrom()) {
        if(move_mode == eLeftEnd) {
            _ASSERT(p == a.front().GetExtendedFrom());               // exon doesn't start from a middle of insertion
            return b.front().GetExtendedFrom();
        } else {
            return -1;
        }
    }

    if(p > a.back().GetTo()) {
        if(move_mode == eRightEnd) {
           _ASSERT(p == a.back().GetExtendedTo());                // exon doesn't start from a middle of insertion
           return b.back().GetExtendedTo();
        } else {
            return -1;
        }
    }
    
    int num = FindLowerRange(a, p);                               // range a[num] exists and its start <= p
                                                                  // if a[num+1] exists all points are > p
    if(p > a[num].GetTo()) {                                      //  between ranges (insertion or intron in a), num+1 exists
        switch(move_mode) {
        case eLeftEnd:
            _ASSERT(p == a[num+1].GetExtendedFrom());             // exon doesn't start from a middle of insertion
            return b[num+1].GetExtendedFrom();
        case eRightEnd:
            _ASSERT(p == a[num].GetExtendedTo());                 // exon doesn't stop in a middle of insertion
            return b[num].GetExtendedTo();
        default:
            return -1;
        }
    } else if(p == a[num].GetTo()) {
        if(move_mode == eRightEnd) {
            _ASSERT(a[num].GetExtraTo() == 0);                 // exon doesn't stop in a middle of insertion
            return b[num].GetExtendedTo();
        } else {
            return b[num].GetTo();
        }
    } else if(p == a[num].GetFrom()) {
        if(move_mode == eLeftEnd) {
            _ASSERT(a[num].GetExtraFrom() == 0);               // exon doesn't start from a middle of insertion
            return b[num].GetExtendedFrom();
        } else {
            return b[num].GetFrom();
        }
    } else {
        return b[num].GetFrom()+p-a[num].GetFrom();
    }
}

TSignedSeqPos CFrameShiftedSeqMap::MapOrigToEdited(TSignedSeqPos orig_pos)  const {
    TSignedSeqPos p = MapAtoB(m_orig_ranges, m_edited_ranges, orig_pos, eSinglePoint);
    
    if(m_strand == eMinus && p >= 0) {
        _ASSERT(m_edited_ranges.front().GetExtendedFrom() == 0);
        int last = m_edited_ranges.back().GetExtendedTo();
        p = last-p;
    }
    return p;
}

TSignedSeqPos CFrameShiftedSeqMap::MapEditedToOrig(TSignedSeqPos edited_pos)  const {
    if(m_strand == eMinus) {
        _ASSERT(m_edited_ranges.front().GetExtendedFrom() == 0);
        int last = m_edited_ranges.back().GetExtendedTo();
        edited_pos = last-edited_pos;
    }

    return MapAtoB(m_edited_ranges, m_orig_ranges, edited_pos, eSinglePoint);
}


TSignedSeqRange CFrameShiftedSeqMap::MapRangeAtoB(const vector<CFrameShiftedSeqMap::SMapRange>& a, const vector<CFrameShiftedSeqMap::SMapRange>& b, TSignedSeqRange r, bool withextras) {

    if(r.Empty()) return TSignedSeqRange::GetEmpty();

    TSignedSeqPos left;
    if(r.GetFrom() == TSignedSeqRange::GetWholeFrom()) {
        left = TSignedSeqRange::GetWholeFrom();
    } else {
        left = MapAtoB(a, b, r.GetFrom(), withextras ? eLeftEnd : eSinglePoint);
        _ASSERT(left >= 0);
    }
    TSignedSeqPos right;
    if(r.GetTo() == TSignedSeqRange::GetWholeTo()) {
        right = TSignedSeqRange::GetWholeTo();
    } else {
        right = MapAtoB(a, b, r.GetTo(), withextras ? eRightEnd : eSinglePoint);
        _ASSERT(right >= 0);
    }

    _ASSERT(right >= left);

    return TSignedSeqRange(left, right);
}

TSignedSeqRange CFrameShiftedSeqMap::MapRangeOrigToEdited(TSignedSeqRange orig_range, bool withextras) const {
    
    if(orig_range.Empty()) return TSignedSeqRange::GetEmpty();

    TSignedSeqRange er = MapRangeAtoB(m_orig_ranges, m_edited_ranges, orig_range, withextras);

    if(m_strand == ePlus) return er;

    _ASSERT(m_edited_ranges.front().GetExtendedFrom() == 0);
    int last = m_edited_ranges.back().GetExtendedTo();
    
    TSignedSeqPos left = er.GetTo();
    TSignedSeqPos right = er.GetFrom();
    
    if(left == TSignedSeqRange::GetWholeTo()) {
        left = TSignedSeqRange::GetWholeFrom();
    } else {
        left = last-left;
    }

    if(right == TSignedSeqRange::GetWholeFrom()) {
        right = TSignedSeqRange::GetWholeTo();
    } else {
        right = last-right;
    }

    return TSignedSeqRange(left, right);
}

TSignedSeqRange CFrameShiftedSeqMap::MapRangeEditedTodOrig(TSignedSeqRange edited_range, bool withextras) const {
    
    if(edited_range.Empty()) return TSignedSeqRange::GetEmpty();

    if(m_strand == eMinus) {
        _ASSERT(m_edited_ranges.front().GetExtendedFrom() == 0);
        int last = m_edited_ranges.back().GetExtendedTo();
        
        TSignedSeqPos left = edited_range.GetTo();
        TSignedSeqPos right = edited_range.GetFrom();
        
        if(left == TSignedSeqRange::GetWholeTo()) {
            left = TSignedSeqRange::GetWholeFrom();
        } else {
            left = last-left;
        }

        if(right == TSignedSeqRange::GetWholeFrom()) {
            right = TSignedSeqRange::GetWholeTo();
        } else {
            right = last-right;
        }

        edited_range = TSignedSeqRange(left, right);
    }

    return MapRangeAtoB(m_edited_ranges, m_orig_ranges, edited_range, withextras);
}


TSignedSeqRange MapRangeToOrig(TSignedSeqPos start, TSignedSeqPos stop, const CFrameShiftedSeqMap& mrnamap)
{
    start = mrnamap.MapEditedToOrig(start);
    stop = mrnamap.MapEditedToOrig(stop);
    _ASSERT(start>=0 && stop>=0); // not out of range and not a deletion, because we converted deletion into insertions
    if (start > stop)
        swap(start, stop);
    return TSignedSeqRange(start, stop);
}



END_SCOPE(gnomon)
END_NCBI_SCOPE
