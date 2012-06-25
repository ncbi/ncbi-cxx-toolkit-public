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
template bool IsStopCodon<TResidue>(const TResidue * seq, int strand);


void FindAllCodonInstances(TIVec positions[], const EResidue codon[], const CEResidueVec& mrna, const CAlignMap& mrnamap, TSignedSeqRange search_region, int fixed_frame)
{
    for (CEResidueVec::const_iterator pos = mrna.begin()+search_region.GetFrom(); (pos = search(pos,mrna.end(),codon,codon+3)) < mrna.begin()+search_region.GetTo(); ++pos) {
        int l = pos-mrna.begin();
        int frame = l%3;
        if (fixed_frame==-1 || fixed_frame==frame)
            positions[frame].push_back(l);
    }
}

void FindAllStarts(TIVec starts[], const CEResidueVec& mrna, const CAlignMap& mrnamap, TSignedSeqRange search_region, int fixed_frame)
{
    FindAllCodonInstances(starts, ecodons[0], mrna, mrnamap, search_region, fixed_frame);
}

void FindAllStops(TIVec stops[], const CEResidueVec& mrna, const CAlignMap& mrnamap, TSignedSeqRange search_region, int fixed_frame)
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

void FindStartsStops(const CGeneModel& model, const CEResidueVec& contig_seq, const CEResidueVec& mrna, const CAlignMap& mrnamap, TIVec starts[3],  TIVec stops[3], int& frame)
{
    int left_cds_limit = -1;
    int reading_frame_start = mrna.size();
    int reading_frame_stop = mrna.size();
    int right_cds_limit = mrna.size();
    frame = -1;
    EStrand strand = model.Strand();

    if (!model.ReadingFrame().Empty()) {
        //        left_cds_limit = mrnamap.MapOrigToEdited(model.GetCdsInfo().MaxCdsLimits().GetFrom());
        //        _ASSERT(left_cds_limit >= 0 || model.GetCdsInfo().MaxCdsLimits().GetFrom() == TSignedSeqRange::GetWholeFrom());
        //        right_cds_limit = mrnamap.MapOrigToEdited(model.GetCdsInfo().MaxCdsLimits().GetTo());
        //        _ASSERT(right_cds_limit >= 0 || model.GetCdsInfo().MaxCdsLimits().GetTo() == TSignedSeqRange::GetWholeTo());

        //        if(strand == eMinus) {
        //            std::swap(left_cds_limit,right_cds_limit);
        //        }
        //        if(right_cds_limit < 0) right_cds_limit = mrna.size();

        TSignedSeqRange rf = mrnamap.MapRangeOrigToEdited(model.ReadingFrame(),true);
        reading_frame_start = rf.GetFrom();
        _ASSERT(reading_frame_start >= 0);
        reading_frame_stop = rf.GetTo();
        _ASSERT(reading_frame_stop >= 0);

        if (reading_frame_start == 0 && IsStartCodon(&mrna[reading_frame_start]) && reading_frame_start+3 < reading_frame_stop)
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

        reading_frame_start = reading_frame_stop-5;              // allow starts inside reading frame if not protein
        if(model.GetCdsInfo().ProtReadingFrame().NotEmpty()) {
            TSignedSeqRange protrf = mrnamap.MapRangeOrigToEdited(model.GetCdsInfo().ProtReadingFrame(),true);
            reading_frame_start = min(protrf.GetFrom(),reading_frame_start);
        }
    }

    if (left_cds_limit<0) {
        int model_start = mrnamap.MapEditedToOrig(0);

        if(Include(model.GetCdsInfo().ProtReadingFrame(),model_start) && reading_frame_start < 3) {
            starts[0].push_back(-3);            // proteins are scored no matter what
        } else {
            if(strand == eMinus)
                model_start = contig_seq.size()-1-model_start; 
            for (int i = 0; i<3; ++i) {
                if (frame == -1 || frame == i) {

                    starts[i].push_back(i-3);

                    /*
                    if (UpstreamStartOrSplice(contig_seq,model_start,i))
                        starts[i].push_back(i-3);
                    else 
                        stops[i].push_back(i-3); // bogus stop to limit MaxCds 
                    */
                }
            }
        }

        left_cds_limit = 0;
    }

    if(reading_frame_start-left_cds_limit >= 3)
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

void PushInDel(TInDels& indels, bool fs_only, TSignedSeqPos p, int len, bool insertion) {
    if(fs_only)
        len %= 3;
    if(len != 0) 
        indels.push_back(CInDelInfo(p, len, insertion));
}

TInDels CAlignMap::GetInDels(bool fs_only) const {
    TInDels indels;

    if(m_orig_ranges.front().GetExtraFrom() > 0) {   // everything starts from insertion
        int len = m_orig_ranges.front().GetExtraFrom();
        TSignedSeqPos p = m_orig_ranges.front().GetFrom()-len;
        PushInDel(indels, fs_only, p, len, true);
    }
    if(m_edited_ranges.front().GetExtraFrom() > 0) {   // everything starts from deletion
        int len = m_edited_ranges.front().GetExtraFrom();
        TSignedSeqPos p = m_orig_ranges.front().GetFrom();
        PushInDel(indels, fs_only, p, len, false);
    }

    for(unsigned int range = 1; range < m_orig_ranges.size(); ++range) {
        if(fs_only) {
            int len = m_orig_ranges[range].GetExtraFrom()-m_edited_ranges[range].GetExtraFrom();
            if(m_orig_ranges[range].GetTypeFrom() != eInDel)
                len += m_orig_ranges[range-1].GetExtraTo()-m_edited_ranges[range-1].GetExtraTo();
            if(len%3 == 0)
                continue;
        }

        if(m_orig_ranges[range-1].GetTypeTo() != eInDel) {
            if(m_orig_ranges[range-1].GetExtraTo() > 0) {
                int len = m_orig_ranges[range-1].GetExtraTo();
                TSignedSeqPos p = m_orig_ranges[range-1].GetTo()+1;
                PushInDel(indels, fs_only, p, len, true);
            }
            if(m_edited_ranges[range-1].GetExtraTo() > 0) {
                int len = m_edited_ranges[range-1].GetExtraTo();
                TSignedSeqPos p = m_orig_ranges[range-1].GetTo()+1;
                PushInDel(indels, fs_only, p, len, false);
            }
        }
        
        if(m_orig_ranges[range].GetExtraFrom() > 0) {   // insertion on the left side   
            int len = m_orig_ranges[range].GetExtraFrom();
            TSignedSeqPos p = m_orig_ranges[range].GetFrom()-len;
            PushInDel(indels, fs_only, p, len, true);
        }
        if(m_edited_ranges[range].GetExtraFrom() > 0) {   // deletion on the left side  
            int len = m_edited_ranges[range].GetExtraFrom();
            TSignedSeqPos p = m_orig_ranges[range].GetFrom();
            PushInDel(indels, fs_only, p, len, false);
        }
    }

    if(m_orig_ranges.back().GetExtraTo() > 0) {   // everything ends by insertion
        int len = m_orig_ranges.back().GetExtraTo();
        TSignedSeqPos p = m_orig_ranges.back().GetTo()+1;
        PushInDel(indels, fs_only, p, len, true);
    }
    if(m_edited_ranges.back().GetExtraTo() > 0) {   // everything ends by deletion
        int len = m_edited_ranges.back().GetExtraTo();
        TSignedSeqPos p = m_orig_ranges.back().GetTo()+1;
        PushInDel(indels, fs_only, p, len, false);
    }

    return indels;
}

void CAlignMap::InsertOneToOneRange(TSignedSeqPos orig_start, TSignedSeqPos edited_start, int len, int left_orige, int left_edite, int right_orige, int right_edite,  EEdgeType left_type, EEdgeType right_type)
{
    _ASSERT(len > 0);
    _ASSERT(m_orig_ranges.empty() || (orig_start > m_orig_ranges.back().GetTo() && edited_start > m_edited_ranges.back().GetTo()));

    CAlignMap::SMapRangeEdge orig_from(orig_start);
    orig_from.m_edge_type = left_type;
    orig_from.m_extra = left_orige;

    CAlignMap::SMapRangeEdge orig_to(orig_start+len-1);
    orig_to.m_edge_type = right_type;
    orig_to.m_extra = right_orige;

    m_orig_ranges.push_back(SMapRange(orig_from, orig_to));

    CAlignMap::SMapRangeEdge edited_from(edited_start);
    edited_from.m_edge_type = orig_from.m_edge_type;
    edited_from.m_extra = left_edite;

    CAlignMap::SMapRangeEdge edited_to(edited_start+len-1);
    edited_to.m_edge_type = orig_to.m_edge_type;
    edited_to.m_extra = right_edite;

    m_edited_ranges.push_back(SMapRange(edited_from, edited_to));
}

/*
void CAlignMap::InsertOneToOneRange(TSignedSeqPos orig_start, TSignedSeqPos edited_start, int len, const CInDelInfo* left_fs, const CInDelInfo* right_fs, EEdgeType left_type, EEdgeType right_type)
{
    _ASSERT(len > 0);
    _ASSERT(m_orig_ranges.empty() || (orig_start > m_orig_ranges.back().GetTo() && edited_start > m_edited_ranges.back().GetTo()));

    CAlignMap::SMapRangeEdge orig_from(orig_start);
    orig_from.m_edge_type = left_type;
    if(left_fs != 0 && left_fs->IsInsertion()) {
        orig_from.m_extra = left_fs->Len();
    }
    CAlignMap::SMapRangeEdge orig_to(orig_start+len-1);
    orig_to.m_edge_type = right_type;
    if(right_fs != 0 && right_fs->IsInsertion()) {
        orig_to.m_extra = right_fs->Len();
    }
    m_orig_ranges.push_back(SMapRange(orig_from, orig_to));

    CAlignMap::SMapRangeEdge edited_from(edited_start);
    edited_from.m_edge_type = orig_from.m_edge_type;
    if(left_fs != 0 && left_fs->IsDeletion()) {
        edited_from.m_extra = left_fs->Len();
    }
    CAlignMap::SMapRangeEdge edited_to(edited_start+len-1);
    edited_to.m_edge_type = orig_to.m_edge_type;
    if(right_fs != 0 && right_fs->IsDeletion()) {
        edited_to.m_extra = right_fs->Len();
    }
    m_edited_ranges.push_back(SMapRange(edited_from, edited_to));
}
*/

TSignedSeqPos CAlignMap::InsertIndelRangesForInterval(TSignedSeqPos orig_a, TSignedSeqPos orig_b, TSignedSeqPos edit_a, TInDels::const_iterator fsi_begin, TInDels::const_iterator fsi_end, EEdgeType type_a, EEdgeType type_b) 
{
    TInDels::const_iterator fsi = fsi_begin;
    for( ;fsi != fsi_end && fsi->Loc() < orig_a; ++fsi ) {
        _ASSERT( !fsi->IntersectingWith(orig_a,orig_b) );
    }

    int left_orige = 0;
    int left_edite = 0;

    for( ;fsi != fsi_end && fsi->Loc() == orig_a; ++fsi ) {    // first left end
        if (fsi->IsInsertion()) {
            orig_a += fsi->Len();
            left_orige += fsi->Len(); 
        } else {
            edit_a += fsi->Len();
            left_edite += fsi->Len();
        }
    }

    while(fsi != fsi_end && fsi->Loc() <= (fsi->IsInsertion() ? orig_b : orig_b+1)) {
        int len = fsi->Loc()-orig_a;
        _ASSERT(len > 0 && orig_a+len-1 <= orig_b);

        int bb = fsi->Loc();
        int right_orige = 0;
        int right_edite = 0;
        for( ;fsi != fsi_end && fsi->Loc() == bb; ++fsi ) {      // right end
            if (fsi->IsInsertion()) {
                right_orige += fsi->Len();
                bb += fsi->Len();
            } else {
                right_edite += fsi->Len();
            }
        }

        int next_orig_a = orig_a+len+right_orige;
        InsertOneToOneRange(orig_a, edit_a, len, left_orige, left_edite, right_orige, right_edite, type_a, (next_orig_a <= orig_b) ? eInDel : type_b);

        orig_a = next_orig_a;
        edit_a += len+right_edite;
        type_a = eInDel;
        left_orige = right_orige;
        left_edite = right_edite;
    }

    if(orig_a <= orig_b) {
        int len = orig_b-orig_a+1;
        _ASSERT(len > 0);
        InsertOneToOneRange(orig_a, edit_a, len, left_orige, left_edite, 0, 0, type_a, type_b);
        edit_a += len;
    }

    return edit_a;
}

/*
TSignedSeqPos CAlignMap::InsertIndelRangesForInterval(TSignedSeqPos orig_a, TSignedSeqPos orig_b, TSignedSeqPos edit_a, TInDels::const_iterator fsi_begin, TInDels::const_iterator fsi_end, EEdgeType type_a, EEdgeType type_b) 
{
    const CInDelInfo* left_fs = 0;
    const CInDelInfo* right_fs = 0;
    for (TInDels::const_iterator fsi = fsi_begin; fsi != fsi_end; ++fsi) {
        TSignedSeqPos blim = fsi->IsInsertion() ? orig_b : orig_b+1;
        if(fsi->Loc() < orig_a) {
            _ASSERT( !fsi->IntersectingWith(orig_a,orig_b) );
            continue;
        } else if(fsi->Loc() > blim) {
            break;
        } else {
            right_fs = &(*fsi);
            int len = fsi->Loc()-orig_a;
            //            _ASSERT(left_fs == 0 || len > 0);                               // no two insertion in a row;

            TSignedSeqPos next_orig_a = orig_a;
            if (fsi->IsInsertion()) 
                next_orig_a += fsi->Len();
            
            if(len > 0) {
                next_orig_a += len;
                InsertOneToOneRange(orig_a, edit_a, len, left_fs, right_fs, type_a, (next_orig_a <= orig_b) ? eInDel : type_b);
                edit_a += len;
                type_a = eInDel;
            }
            if (fsi->IsDeletion()) 
                edit_a += fsi->Len();
            orig_a = next_orig_a;
            left_fs = right_fs;
        }
    }
    if(orig_a <= orig_b) {
        int len = orig_b-orig_a+1;
        InsertOneToOneRange(orig_a, edit_a, len, left_fs, 0, type_a, type_b);
        edit_a += len;
    }

    return edit_a;
}
*/

CAlignMap::CAlignMap(const CGeneModel::TExons& exons, const vector<TSignedSeqRange>& transcript_exons, const TInDels& indels, EStrand orientation, int target_len ) : m_orientation(orientation), m_target_len(target_len) {

#ifdef _DEBUG
    _ASSERT(transcript_exons.size() == exons.size());
    _ASSERT(transcript_exons.size() == 1 || (orientation == ePlus && transcript_exons.front().GetFrom() < transcript_exons.back().GetFrom()) ||
           (orientation == eMinus && transcript_exons.front().GetFrom() > transcript_exons.back().GetFrom()));
    int diff = 0;
    for(unsigned int i = 0; i < exons.size(); ++i) {
        diff += exons[i].GetTo()-exons[i].GetFrom()-transcript_exons[i].GetTo()+transcript_exons[i].GetFrom();
    }
    ITERATE(TInDels, f, indels) {
        diff += (f->IsDeletion()) ? f->Len() : -f->Len();
    }
    _ASSERT(diff == 0);
#endif

    m_orig_ranges.reserve(exons.size()+indels.size());
    m_edited_ranges.reserve(exons.size()+indels.size());

    TSignedSeqPos estart = (m_orientation == ePlus) ? transcript_exons.front().GetFrom() : transcript_exons.back().GetFrom();
    for(unsigned int i = 0; i < exons.size(); ++i) {
        EEdgeType type_a = exons[i].m_fsplice ? eSplice : eBoundary;
        EEdgeType type_b = exons[i].m_ssplice ? eSplice : eBoundary;
        estart = InsertIndelRangesForInterval(exons[i].GetFrom(), exons[i].GetTo(), estart, indels.begin(), indels.end(), type_a, type_b);
        if(i != exons.size()-1) {
            if(m_orientation == ePlus) {
                estart += transcript_exons[i+1].GetFrom()-transcript_exons[i].GetTo()-1;
            } else {
                estart += transcript_exons[i].GetFrom()-transcript_exons[i+1].GetTo()-1;
            }
        }
    }
}

CAlignMap::CAlignMap(const CGeneModel::TExons& exons, const TInDels& frameshifts, EStrand strand, TSignedSeqRange lim, int holelen, int polyalen) : m_orientation(strand) { 

    TInDels::const_iterator fsi_begin = frameshifts.begin();
    TInDels::const_iterator fsi_end = frameshifts.end();

    m_orig_ranges.reserve(exons.size()+(fsi_end-fsi_begin));
    m_edited_ranges.reserve(exons.size()+(fsi_end-fsi_begin));
    
    TSignedSeqPos estart = 0;
    for(unsigned int i = 0; i < exons.size(); ++i) {
        TSignedSeqPos start = exons[i].GetFrom();
        TSignedSeqPos stop = exons[i].GetTo();
        EEdgeType type_a = exons[i].m_fsplice ? eSplice : eBoundary;
        EEdgeType type_b = exons[i].m_ssplice ? eSplice : eBoundary;
        
        if(stop < lim.GetFrom()) continue;
        if(lim.GetTo() < start) break;
        
        if(lim.GetFrom() >= start) {
            start = lim.GetFrom();
            type_a = eBoundary;
        }
        if(lim.GetTo() <= stop) {
            stop = lim.GetTo();
            type_b = eBoundary;
        }

        estart = InsertIndelRangesForInterval(start, stop, estart, fsi_begin, fsi_end, type_a, type_b);
        if(i != exons.size()-1 && (!exons[i+1].m_fsplice || !exons[i].m_ssplice)) 
            estart += holelen;
    }

    if(!m_edited_ranges.empty())
        m_target_len = m_edited_ranges.back().GetExtendedTo()+1+polyalen;

    _ASSERT(m_edited_ranges.size() == m_orig_ranges.size());
}

template <class Vec>
void CAlignMap::EditedSequence(const Vec& original_sequence, Vec& edited_sequence, bool includeholes) const
{
    edited_sequence.clear();

    int l = 0;
    if(includeholes) {
        if(m_orientation == ePlus)
            l = m_edited_ranges.front().GetFrom();
        else
            l = CAlignMap::TargetLen()-m_edited_ranges.back().GetTo()-1;
    } else {
        l = m_edited_ranges.front().GetExtraFrom();
    }
    edited_sequence.insert(edited_sequence.end(),l,res_traits<typename Vec::value_type>::_fromACGT('N'));    
    for(int range = 0; range < (int)m_orig_ranges.size(); ++range) {
        int a = m_orig_ranges[range].GetFrom();
        int b = m_orig_ranges[range].GetTo()+1;
        edited_sequence.insert(edited_sequence.end(),original_sequence.begin()+a, original_sequence.begin()+b);

        int l = 0;
        if(range < (int)m_orig_ranges.size()-1) {
            if(includeholes) 
                l = m_edited_ranges[range+1].GetFrom()-m_edited_ranges[range].GetTo()-1;                  // missed part
            else if(m_edited_ranges[range].GetExtendedTo() < m_edited_ranges[range+1].GetExtendedFrom())
                l = m_edited_ranges[range].GetExtraTo() + m_edited_ranges[range+1].GetExtraFrom();        // indels from two exon ends
            else
                l = m_edited_ranges[range].GetExtraTo();                                                  // indel inside exon

        } else {
            if(includeholes) {
                if(m_orientation == ePlus) 
                    l = CAlignMap::TargetLen()-m_edited_ranges.back().GetTo()-1;
                else
                    l = m_edited_ranges.front().GetFrom();
            } else {
                l = m_edited_ranges.back().GetExtraTo();
            }
        }
        edited_sequence.insert(edited_sequence.end(),l,res_traits<typename Vec::value_type>::_fromACGT('N'));
    }
    
    if(m_orientation == eMinus) 
        ReverseComplement(edited_sequence.begin(), edited_sequence.end());
}

int CAlignMap::FindLowerRange(const vector<CAlignMap::SMapRange>& a,  TSignedSeqPos p) {
    int num = lower_bound(a.begin(), a.end(), CAlignMap::SMapRange(p+1,p+1))-a.begin()-1;
    return num;
}

template
void CAlignMap::EditedSequence<CResidueVec>(const CResidueVec& original_sequence, CResidueVec& edited_sequence, bool includeholes) const;
template
void CAlignMap::EditedSequence<CEResidueVec>(const CEResidueVec& original_sequence, CEResidueVec& edited_sequence, bool includeholes) const;


TSignedSeqRange CAlignMap::ShrinkToRealPoints(TSignedSeqRange orig_range, bool snap_to_codons) const {

    _ASSERT(orig_range.NotEmpty() && Include(TSignedSeqRange(m_orig_ranges.front().GetExtendedFrom(),m_orig_ranges.back().GetExtendedTo()), orig_range));

    TSignedSeqPos a = orig_range.GetFrom();
    int numa =  FindLowerRange(m_orig_ranges, a);

    if(numa < 0 || a > m_orig_ranges[numa].GetTo()) {   // a was insertion on the genome, moved to first projectable point
        ++numa;
        if((int)m_orig_ranges.size() == numa)
            return TSignedSeqRange::GetEmpty();
        a = m_orig_ranges[numa].GetFrom();
    }
    if(snap_to_codons) {
        bool snapped = false;
        while(!snapped) {
            TSignedSeqPos tp = m_edited_ranges[numa].GetFrom()+a-m_orig_ranges[numa].GetFrom(); 
            if(m_orientation == eMinus) 
                tp = m_edited_ranges.front().GetFrom()+m_edited_ranges.back().GetTo()-tp;
            if((m_orientation == ePlus && tp%3 == 0) || (m_orientation == eMinus && tp%3 == 2)) {
                snapped = true;
            } else {                                       // not a codon boundary
                if(a < m_orig_ranges[numa].GetTo()) {      // can move in this interval
                    ++a;
                } else {                                   // moved to next interval
                    ++numa;
                    if((int)m_orig_ranges.size() == numa)
                        return TSignedSeqRange::GetEmpty();
                    a = m_orig_ranges[numa].GetFrom();
                }
            }
        }
    }
    

    TSignedSeqPos b = orig_range.GetTo();
    int numb =  FindLowerRange(m_orig_ranges, b);

    if(b > m_orig_ranges[numb].GetTo()) {   // a was insertion on the genome, moved to first projectable point
       b = m_orig_ranges[numb].GetTo();
    }
    if(snap_to_codons) {
        bool snapped = false;
        while(!snapped) {
            TSignedSeqPos tp = m_edited_ranges[numb].GetFrom()+b-m_orig_ranges[numb].GetFrom();
            if(m_orientation == eMinus) 
                tp = m_edited_ranges.front().GetFrom()+m_edited_ranges.back().GetTo()-tp;
            if((m_orientation == ePlus && tp%3 == 2) || (m_orientation == eMinus && tp%3==0)) {
                snapped = true;
            } else {                                       // not a codon boundary
                if(b > m_orig_ranges[numb].GetFrom()) {      // can move in this interval        
                    --b;
                } else {                                   // moved to next interval
                    --numb;
                    if(numb < 0)
                        return TSignedSeqRange::GetEmpty();
                    b = m_orig_ranges[numb].GetTo();
                }
            }
        }
    }

    return TSignedSeqRange(a, b);
}

TSignedSeqPos CAlignMap::FShiftedMove(TSignedSeqPos orig_pos, int len) const {
    orig_pos = MapOrigToEdited(orig_pos);
    if(orig_pos < 0) 
        return orig_pos;
    if(m_orientation == ePlus)
        orig_pos += len;
    else
        orig_pos -= len;
    orig_pos = MapEditedToOrig(orig_pos);
    return orig_pos;
}


TSignedSeqPos CAlignMap::MapAtoB(const vector<CAlignMap::SMapRange>& a, const vector<CAlignMap::SMapRange>& b, TSignedSeqPos p, ERangeEnd move_mode) {
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
            //            _ASSERT(p == a[num+1].GetExtendedFrom());       CDS may do it      // exon doesn't start from a middle of insertion
            return b[num+1].GetExtendedFrom();
        case eRightEnd:
            //            _ASSERT(p == a[num].GetExtendedTo());                 // exon doesn't stop in a middle of insertion
            return b[num].GetExtendedTo();
        default:
            return -1;
        }
    } else if(p == a[num].GetTo()) {
        if(move_mode == eRightEnd) {
            //            _ASSERT(a[num].GetExtraTo() == 0);                 // exon doesn't stop in a middle of insertion
            return b[num].GetExtendedTo();
        } else if(p == a[num].GetFrom() && move_mode == eLeftEnd) {          // one base interval
            return b[num].GetExtendedFrom();
        } else {
            return b[num].GetTo();
        }
    } else if(p == a[num].GetFrom()) {
        if(move_mode == eLeftEnd) {
            //            _ASSERT(a[num].GetExtraFrom() == 0);               // exon doesn't start from a middle of insertion
            return b[num].GetExtendedFrom();
        } else {
            return b[num].GetFrom();
        }
    } else {
        return b[num].GetFrom()+p-a[num].GetFrom();
    }
}

TSignedSeqPos CAlignMap::MapOrigToEdited(TSignedSeqPos orig_pos)  const {
    TSignedSeqPos p = MapAtoB(m_orig_ranges, m_edited_ranges, orig_pos, eSinglePoint);
    
    if(m_orientation == eMinus && p >= 0) {
        p = m_edited_ranges.front().GetFrom()+m_edited_ranges.back().GetTo()-p;
    }
    return p;
}

TSignedSeqPos CAlignMap::MapEditedToOrig(TSignedSeqPos edited_pos)  const {
    if(m_orientation == eMinus) {
        edited_pos = m_edited_ranges.front().GetFrom()+m_edited_ranges.back().GetTo()-edited_pos;
    }

    return MapAtoB(m_edited_ranges, m_orig_ranges, edited_pos, eSinglePoint);
}


TSignedSeqRange CAlignMap::MapRangeAtoB(const vector<CAlignMap::SMapRange>& a, const vector<CAlignMap::SMapRange>& b, TSignedSeqRange r, ERangeEnd lend, ERangeEnd rend) {

    if(r.Empty()) return TSignedSeqRange::GetEmpty();

    TSignedSeqPos left;
    if(r.GetFrom() == TSignedSeqRange::GetWholeFrom()) {
        left = TSignedSeqRange::GetWholeFrom();
    } else {
        left = MapAtoB(a, b, r.GetFrom(), lend);
        _ASSERT(left >= 0);
    }
    TSignedSeqPos right;
    if(r.GetTo() == TSignedSeqRange::GetWholeTo()) {
        right = TSignedSeqRange::GetWholeTo();
    } else {
        right = MapAtoB(a, b, r.GetTo(), rend);
        _ASSERT(right >= 0);
    }

    _ASSERT(right >= left);

    return TSignedSeqRange(left, right);
}

TSignedSeqRange CAlignMap::MapRangeOrigToEdited(TSignedSeqRange orig_range, ERangeEnd lend, ERangeEnd rend) const {
    
    if(orig_range.Empty()) return TSignedSeqRange::GetEmpty();

    TSignedSeqRange er = MapRangeAtoB(m_orig_ranges, m_edited_ranges, orig_range, lend, rend);

    if(m_orientation == ePlus) return er;

    int offset = m_edited_ranges.front().GetFrom()+m_edited_ranges.back().GetTo();
    TSignedSeqPos left = er.GetTo();
    TSignedSeqPos right = er.GetFrom();
    
    if(left == TSignedSeqRange::GetWholeTo()) {
        left = TSignedSeqRange::GetWholeFrom();
    } else {
        left = offset-left;
    }

    if(right == TSignedSeqRange::GetWholeFrom()) {
        right = TSignedSeqRange::GetWholeTo();
    } else {
        right = offset-right;
    }

    return TSignedSeqRange(left, right);
}

TSignedSeqRange CAlignMap::MapRangeEditedToOrig(TSignedSeqRange edited_range, bool withextras) const {
    
    if(edited_range.Empty()) return TSignedSeqRange::GetEmpty();

    if(m_orientation == eMinus) {
        int offset = m_edited_ranges.front().GetFrom()+m_edited_ranges.back().GetTo();
        TSignedSeqPos left = edited_range.GetTo();
        TSignedSeqPos right = edited_range.GetFrom();
        
        if(left == TSignedSeqRange::GetWholeTo()) {
            left = TSignedSeqRange::GetWholeFrom();
        } else {
            left = offset-left;
        }

        if(right == TSignedSeqRange::GetWholeFrom()) {
            right = TSignedSeqRange::GetWholeTo();
        } else {
            right = offset-right;
        }

        edited_range = TSignedSeqRange(left, right);
    }

    return MapRangeAtoB(m_edited_ranges, m_orig_ranges, edited_range, withextras);
}



END_SCOPE(gnomon)
END_NCBI_SCOPE
