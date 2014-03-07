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
* Author: Aleksey Grichenko
*
* File Description:
*   Seq-loc mapper base
*
*/

#include <ncbi_pch.hpp>
#include <objects/seq/seq_loc_mapper_base.hpp>
#include <objects/seq/seq_align_mapper_base.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/misc/error_codes.hpp>
#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objects_SeqLocMap


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


const char* CAnnotMapperException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eBadLocation:      return "eBadLocation";
    case eUnknownLength:    return "eUnknownLength";
    case eBadAlignment:     return "eBadAlignment";
    case eBadFeature:       return "eBadFeature";
    case eOtherError:       return "eOtherError";
    default:                return CException::GetErrCodeString();
    }
}

/*
/////////////////////////////////////////////////////////////////////

CSeq_loc_Mapper_Base basic approaches.

1. Initialization

The mapper parses input data (two seq-locs, seq-alignment) and stores
mappings in a collection of CMappingRange objects. Each mapping range
contains source (id, start, stop, strand) and destination (id, start,
strand).

All coordinates are converted to genomic with one exception: if
source and destination locations have the same length and the mapper
can not obtain real sequence types, it assumes that both sequences
are nucleotides even if they are proteins. See x_AdjustSeqTypesToProt()
for more info on this special case.

The mapper uses several methods to check sequence types: by comparing
source and destination lengths, by calling GetSeqType() which is
overriden in CSeq_loc_Mapper to provide the correct information, using
some information from alignments (e.g. spiced-segs contain explicit
sequence types). If all these methods fail, the mapper may still
successfully do its job. E.g. if mapping is between two whole seq-locs,
it may be done with the assumption that both sequences have the same
type.

The order of mapping ranges is not preserved, they are sorted by
source seq-id and start position.

When parsing input locations the mapper also tries to create equivalent
mappings for all synonyms of the source sequence id. The base class
does not provide synonyms, buy CSeq_loc_Mapper does override
CollectSynonyms() method to implement this.

In some situations (like mapping between a bioseq and its segments),
the mapper also creates dummy mappings from destination to itself,
so that during the mapping any ranges already on the destination
sequence are not truncated. See x_PreserveDestinationLocs().


2. Mapping

Mapping of seq-locs is done range-by-range, the original seq-loc
is not parsed completely before mapping. Each original interval is
mapped through all matching mapping ranges, some parts may be mapped
more than once.

The mapped ranges are first stored in a container of SMappedRange
structures. This is done to simplify merging ranges. If no merge
flag is set or the new range can not be merged with the collected
set, all ranges from the container are moved (pushed) to the
destination seq-loc and the new range starts the new collection.
This is done by x_PushMappedRange method (adding a new range) and
x_PushRangesToDstMix (pushing the collected mapped ranges to the
destination seq-loc).

The pushing also occurs in the following situations:
- When a source range is discarded (not just clipped) - see
  x_SetLastTruncated.
- When a non-mapping range is copied to the destination mix (in fact,
  in this case pushing is usually done by the truncation described
  above).
- When a new complex seq-loc is started (e.g. a new mix or equiv)
  to preserve the structure of the source location.

Since merging is done only among the temporary collection, any
of the above conditions breaks merging. Examples:
- The original seq-loc is a mix, containing two other mixes A and B,
  which contain overlapping ranges. These ranges will not be merged,
  since they originate from different complex locations.
- If the original seq-loc contains three ranges A, B and C, which are
  mapped so that A' and C' overlap or abut, but B is discarded, the
  A' and C' will not be merged. Depending on the flags, B may be
  also included in the mapped location between A' and C' (see
  KeepNonmappingRanges).

TODO: Is the above behavior expected or should it be changed so that
merging can be done at least in some of the described cases?

After mapping the destination seq-loc may be a simple interval or
a mix of sub-locations. This mix can be optimized when the mapping
is finished: null locations are removed (if no GapPreserve is set),
as well as empty mixes etc. Mixes with a single element are replaced
with this element. Mixes which contain only intervals are converted
to packed-ints.


/////////////////////////////////////////////////////////////////////
*/


/////////////////////////////////////////////////////////////////////
//
// CDefault_Mapper_Sequence_Info
//
//   Default sequence type/length/synonyms provider - returns unknown type
//   and length for any sequence, adds no synonyms except the original id.


class CDefault_Mapper_Sequence_Info : public IMapper_Sequence_Info
{
public:
    virtual TSeqType GetSequenceType(const CSeq_id_Handle& idh)
        { return CSeq_loc_Mapper_Base::eSeq_unknown; }
    virtual TSeqPos GetSequenceLength(const CSeq_id_Handle& idh)
        { return kInvalidSeqPos; }
    virtual void CollectSynonyms(const CSeq_id_Handle& id,
                                 TSynonyms&            synonyms)
        { synonyms.insert(id); }
};


/////////////////////////////////////////////////////////////////////
//
// CMappingRange
//
//   Helper class for mapping points, ranges, strands and fuzzes
//


CMappingRange::CMappingRange(CSeq_id_Handle     src_id,
                             TSeqPos            src_from,
                             TSeqPos            src_length,
                             ENa_strand         src_strand,
                             CSeq_id_Handle     dst_id,
                             TSeqPos            dst_from,
                             ENa_strand         dst_strand,
                             bool               ext_to,
                             int                frame,
                             TSeqPos            dst_total_len,
                             TSeqPos            src_bioseq_len,
                             TSeqPos            dst_len)
    : m_Src_id_Handle(src_id),
      m_Src_from(src_from),
      m_Src_to(src_from + src_length - 1),
      m_Src_strand(src_strand),
      m_Dst_id_Handle(dst_id),
      m_Dst_from(dst_from),
      m_Dst_strand(dst_strand),
      m_Reverse(!SameOrientation(src_strand, dst_strand)),
      m_ExtTo(ext_to),
      m_Frame(frame),
      m_Dst_total_len(dst_total_len),
      m_Src_bioseq_len(src_bioseq_len),
      m_Dst_len(dst_len),
      m_Group(0)
{
    return;
}


bool CMappingRange::CanMap(TSeqPos    from,
                           TSeqPos    to,
                           bool       is_set_strand,
                           ENa_strand strand) const
{
    // The callers set is_set_strand to true only if the mapper's
    // m_CheckStrand is enabled. Only in this case CanMap() checks
    // if the location's strand is the same as the mapping's one.
    if ( is_set_strand  &&  (IsReverse(strand) != IsReverse(m_Src_strand)) ) {
        return false;
    }
    return from <= m_Src_to  &&  to >= m_Src_from;
}


TSeqPos CMappingRange::Map_Pos(TSeqPos pos) const
{
    _ASSERT(pos >= m_Src_from  &&  pos <= m_Src_to);
    if (!m_Reverse) {
        return m_Dst_from + pos - m_Src_from;
    }
    else {
        return m_Dst_from + m_Src_to - pos;
    }
}


CMappingRange::TRange CMappingRange::Map_Range(TSeqPos           from,
                                               TSeqPos           to,
                                               const TRangeFuzz* fuzz) const
{
    // Special case of mapping from a protein to a nucleotide through
    // a partial cd-region. Extend the mapped interval to the end of
    // destination range if all of the following conditions are true:
    // - source is a protein (m_ExtTo)
    // - destination is a nucleotide (m_ExtTo)
    // - destination interval has partial "to" (m_ExtTo)
    // - interval to be mapped has partial "to"
    // - destination range is 1 or 2 bases beyond the end of the source range
    const int frame_shift = ( (m_Frame > 1) ? (m_Frame - 1) : 0 );

    // If we're partial on the left and we're not at the beginning only because of 
    // frame shift, we shift back to the beginning when mapping.
    // example accession: AJ237662.1
    const bool partial_from = fuzz  &&  fuzz->first  &&  fuzz->first->IsLim()  &&
            ( fuzz->first->GetLim() == CInt_fuzz::eLim_lt || fuzz->first->GetLim() == CInt_fuzz::eLim_gt );
    const bool partial_to = fuzz  &&  fuzz->second  &&  fuzz->second->IsLim()  &&
            ( fuzz->second->GetLim() == CInt_fuzz::eLim_lt || fuzz->second->GetLim() == CInt_fuzz::eLim_gt );

    from = max(from, m_Src_from);
    to = min(to, m_Src_to);

    if (!m_Reverse) {
        TRange ret(Map_Pos(from), Map_Pos(to));
        // extend to beginning if necessary
        // example accession that triggers this "if": AJ237662.1
        if( (frame_shift > 0) && partial_from && (from == 0) && (m_Src_from == 0) ) {
            if( m_Dst_from >= static_cast<TSeqPos>(frame_shift) ) {
                ret.SetFrom( m_Dst_from - frame_shift );
            } else {
                ret.SetFrom( m_Dst_from );
            }
        }
        // extend to the end, if necessary
        if( m_Dst_len != kInvalidSeqPos ) {
            const TSeqPos src_to_dst_end = m_Dst_from + (m_Src_to - m_Src_from);
            const TSeqPos new_dst_end    = m_Dst_from + m_Dst_len - 1;    
            if ( m_ExtTo && partial_to && to == m_Src_bioseq_len ) {
                if( ((int)new_dst_end - (int)src_to_dst_end) >= 0 && (new_dst_end - src_to_dst_end) <= 2 ) {
                    ret.SetTo( new_dst_end );
                }
            }
        }
        return ret;
    }
    else {
        TRange ret(Map_Pos(to), Map_Pos(from));

        // extend to beginning if necessary (Note: reverse strand implies "beginning" is a higher number )
        if( m_Dst_len != kInvalidSeqPos ) {
            const TSeqPos new_dst_end  = m_Dst_from + m_Dst_len - 1;    
            if ( (frame_shift > 0) && partial_from && (from == 0) && (m_Src_from == 0) ) {
                ret.SetTo( new_dst_end + frame_shift );
            }
        }
        // extend to the end, if necessary (Note: reverse strand implies "end" is a lower number )
        // ( e.g. NZ_AAOJ01000043 )
        if( m_ExtTo && partial_to && (to == m_Src_bioseq_len) ) {
            ret.SetFrom( m_Dst_from );
        }

        return ret;
    }
}


bool CMappingRange::Map_Strand(bool        is_set_strand,
                               ENa_strand  src,
                               ENa_strand* dst) const
{
    _ASSERT(dst);
    if ( m_Reverse ) {
        // Always convert to reverse strand, even if the source
        // strand is unknown.
        *dst = Reverse(src);
        return true;
    }
    if (is_set_strand) {
        // Use original strand if set
        *dst = src;
        return true;
    }
    if (m_Dst_strand != eNa_strand_unknown) {
        // Destination strand may be set for nucleotides
        // even if the source one is not set.
        *dst = m_Dst_strand;
        return true;
    }
    return false; // Leave the mapped strand unset.
}


const CMappingRange::TFuzz kEmptyFuzz(0);

CInt_fuzz::ELim CMappingRange::x_ReverseFuzzLim(CInt_fuzz::ELim lim) const
{
    // Recalculate fuzz of type lim to the reverse strand.
    switch ( lim ) {
    case CInt_fuzz::eLim_gt:
        return CInt_fuzz::eLim_lt;
    case CInt_fuzz::eLim_lt:
        return CInt_fuzz::eLim_gt;
    case CInt_fuzz::eLim_tr:
        return CInt_fuzz::eLim_tl;
    case CInt_fuzz::eLim_tl:
        return CInt_fuzz::eLim_tr;
    default:
        return lim;
    }
}


void CMappingRange::x_Map_Fuzz(TFuzz& fuzz) const
{
    if ( !fuzz ) return;
    switch ( fuzz->Which() ) {
    case CInt_fuzz::e_Lim:
        {
            // gt/lt are swapped when mapping to reverse strand.
            if ( m_Reverse ) {
                CRef<CInt_fuzz> oldFuzz = fuzz;
                fuzz.Reset( new CInt_fuzz ); // careful: other TRangeFuzz's may map to the same TFuzz
                fuzz->Assign( *oldFuzz );
                fuzz->SetLim(x_ReverseFuzzLim(fuzz->GetLim()));
            }
            break;
        }
    case CInt_fuzz::e_Alt:
        {
            // Map each point to the destination sequence.
            // Discard non-mappable points (???).
            TFuzz mapped(new CInt_fuzz);
            CInt_fuzz::TAlt& alt = mapped->SetAlt();
            ITERATE(CInt_fuzz::TAlt, it, fuzz->GetAlt()) {
                if ( CanMap(*it, *it, false, eNa_strand_unknown) ) {
                    alt.push_back(Map_Pos(*it));
                }
            }
            if ( !alt.empty() ) {
                fuzz = mapped;
            }
            else {
                fuzz.Reset();
            }
            break;
        }
    case CInt_fuzz::e_Range:
        {
            // Map each range, truncate the ends if necessary.
            // Discard unmappable ranges (???).
            TRange rg(fuzz->GetRange().GetMin(), fuzz->GetRange().GetMax());
            if ( CanMap(rg.GetFrom(), rg.GetTo(), false, eNa_strand_unknown) ) {
                rg = Map_Range(rg.GetFrom(), rg.GetTo());
                if ( !rg.Empty() ) {
                    CRef<CInt_fuzz> oldFuzz = fuzz;
                    fuzz.Reset( new CInt_fuzz ); // careful: other TRangeFuzz's may map to the same TFuzz
                    fuzz->Assign( *oldFuzz );
                    fuzz->SetRange().SetMin(rg.GetFrom());
                    fuzz->SetRange().SetMax(rg.GetTo());
                }
            }
            else {
                rg = TRange::GetEmpty();
            }
            if ( rg.Empty() ) {
                fuzz.Reset();
            }
            break;
        }
    default:
        // Other types are not converted
        break;
    }
}


CMappingRange::TRangeFuzz CMappingRange::Map_Fuzz(const TRangeFuzz& fuzz) const
{
    // Maps fuzz if possible.
    TRangeFuzz res = m_Reverse ? TRangeFuzz(fuzz.second, fuzz.first) : fuzz;
    x_Map_Fuzz(res.first);
    x_Map_Fuzz(res.second);
    return res;
}


/////////////////////////////////////////////////////////////////////
//
// CMappingRanges
//
//   Collection of mapping ranges


CMappingRanges::CMappingRanges(void)
    : m_ReverseSrc(false),
      m_ReverseDst(false)
{
}


void CMappingRanges::AddConversion(CRef<CMappingRange> cvt)
{
    m_IdMap[cvt->m_Src_id_Handle].insert(TRangeMap::value_type(
        TRange(cvt->m_Src_from, cvt->m_Src_to), cvt));
}


CRef<CMappingRange>
CMappingRanges::AddConversion(CSeq_id_Handle    src_id,
                              TSeqPos           src_from,
                              TSeqPos           src_length,
                              ENa_strand        src_strand,
                              CSeq_id_Handle    dst_id,
                              TSeqPos           dst_from,
                              ENa_strand        dst_strand,
                              bool              ext_to,
                              int               frame,
                              TSeqPos           dst_total_len,
                              TSeqPos           src_bioseq_len,
                              TSeqPos           dst_len)
{
    CRef<CMappingRange> cvt(new CMappingRange(
        src_id, src_from, src_length, src_strand,
        dst_id, dst_from, dst_strand,
        ext_to, frame, dst_total_len, src_bioseq_len, dst_len )); 
    AddConversion(cvt);
    return cvt;
}


CMappingRanges::TRangeIterator
CMappingRanges::BeginMappingRanges(CSeq_id_Handle id,
                                   TSeqPos        from,
                                   TSeqPos        to) const
{
    // Get mappings iterator for the given id and range.
    TIdMap::const_iterator ranges = m_IdMap.find(id);
    if (ranges == m_IdMap.end()) {
        return TRangeIterator();
    }
    return ranges->second.begin(TRange(from, to));
}


/////////////////////////////////////////////////////////////////////
//
// CSeq_loc_Mapper_Base
//


/////////////////////////////////////////////////////////////////////
//
//   Initialization of the mapper
//


// Helpers for converting strand to/from index.
// The index is used to access elements of a vector, grouping
// mapping ranges by strand.
inline
ENa_strand s_IndexToStrand(size_t idx)
{
    _ASSERT(idx != 0);
    return ENa_strand(idx - 1);
}

#define STRAND_TO_INDEX(is_set, strand) \
    ((is_set) ? size_t((strand) + 1) : 0)

#define INDEX_TO_STRAND(idx) \
    s_IndexToStrand(idx)


CSeq_loc_Mapper_Base::CSeq_loc_Mapper_Base(IMapper_Sequence_Info* seqinfo)
    : m_MergeFlag(eMergeNone),
      m_GapFlag(eGapPreserve),
      m_KeepNonmapping(false),
      m_CheckStrand(false),
      m_IncludeSrcLocs(false),
      m_Partial(false),
      m_LastTruncated(false),
      m_Mappings(new CMappingRanges),
      m_CurrentGroup(0),
      m_FuzzOption(0),
      m_MapOptions(0),
      m_SeqInfo(seqinfo ? seqinfo : new CDefault_Mapper_Sequence_Info)
{
}


CSeq_loc_Mapper_Base::CSeq_loc_Mapper_Base(CMappingRanges* mapping_ranges,
                                           IMapper_Sequence_Info* seq_info)
    : m_MergeFlag(eMergeNone),
      m_GapFlag(eGapPreserve),
      m_KeepNonmapping(false),
      m_CheckStrand(false),
      m_IncludeSrcLocs(false),
      m_Partial(false),
      m_LastTruncated(false),
      m_Mappings(mapping_ranges),
      m_CurrentGroup(0),
      m_FuzzOption(0),
      m_MapOptions(0),
      m_SeqInfo(seq_info ? seq_info : new CDefault_Mapper_Sequence_Info)
{
}


CSeq_loc_Mapper_Base::CSeq_loc_Mapper_Base(const CSeq_feat&  map_feat,
                                           EFeatMapDirection dir,
                                           IMapper_Sequence_Info* seq_info)
    : m_MergeFlag(eMergeNone),
      m_GapFlag(eGapPreserve),
      m_KeepNonmapping(false),
      m_CheckStrand(false),
      m_IncludeSrcLocs(false),
      m_Partial(false),
      m_LastTruncated(false),
      m_Mappings(new CMappingRanges),
      m_CurrentGroup(0),
      m_FuzzOption(0),
      m_MapOptions(0),
      m_SeqInfo(seq_info ? seq_info : new CDefault_Mapper_Sequence_Info)
{
    x_InitializeFeat(map_feat, dir);
}


CSeq_loc_Mapper_Base::CSeq_loc_Mapper_Base(const CSeq_loc& source,
                                           const CSeq_loc& target,
                                           IMapper_Sequence_Info* seq_info)
    : m_MergeFlag(eMergeNone),
      m_GapFlag(eGapPreserve),
      m_KeepNonmapping(false),
      m_CheckStrand(false),
      m_IncludeSrcLocs(false),
      m_Partial(false),
      m_LastTruncated(false),
      m_Mappings(new CMappingRanges),
      m_CurrentGroup(0),
      m_FuzzOption(0),
      m_MapOptions(0),
      m_SeqInfo(seq_info ? seq_info : new CDefault_Mapper_Sequence_Info)
{
    x_InitializeLocs(source, target);
}


CSeq_loc_Mapper_Base::CSeq_loc_Mapper_Base(const CSeq_align& map_align,
                                           const CSeq_id&    to_id,
                                           TMapOptions       opts,
                                           IMapper_Sequence_Info* seq_info)
    : m_MergeFlag(eMergeNone),
      m_GapFlag(eGapPreserve),
      m_KeepNonmapping(false),
      m_CheckStrand(false),
      m_IncludeSrcLocs(false),
      m_Partial(false),
      m_LastTruncated(false),
      m_Mappings(new CMappingRanges),
      m_CurrentGroup(0),
      m_FuzzOption(0),
      m_MapOptions(opts),
      m_SeqInfo(seq_info ? seq_info : new CDefault_Mapper_Sequence_Info)
{
    x_InitializeAlign(map_align, to_id);
}


CSeq_loc_Mapper_Base::CSeq_loc_Mapper_Base(const CSeq_align& map_align,
                                           size_t            to_row,
                                           TMapOptions       opts,
                                           IMapper_Sequence_Info* seq_info)
    : m_MergeFlag(eMergeNone),
      m_GapFlag(eGapPreserve),
      m_KeepNonmapping(false),
      m_CheckStrand(false),
      m_IncludeSrcLocs(false),
      m_Partial(false),
      m_LastTruncated(false),
      m_Mappings(new CMappingRanges),
      m_CurrentGroup(0),
      m_FuzzOption(0),
      m_MapOptions(opts),
      m_SeqInfo(seq_info ? seq_info : new CDefault_Mapper_Sequence_Info)
{
    x_InitializeAlign(map_align, to_row);
}


CSeq_loc_Mapper_Base::~CSeq_loc_Mapper_Base(void)
{
    return;
}

void CSeq_loc_Mapper_Base::SetFuzzOption( TFuzzOption newOption )
{
    m_FuzzOption = newOption;
}

void CSeq_loc_Mapper_Base::x_InitializeFeat(const CSeq_feat&  map_feat,
                                            EFeatMapDirection dir)
{
    // Make sure product is set
    _ASSERT(map_feat.IsSetProduct());

    // Sometimes sequence types can be detected based on the feature type.
    ESeqType loc_type = eSeq_unknown;
    ESeqType prod_type = eSeq_unknown;
    switch ( map_feat.GetData().Which() ) {
    case CSeqFeatData::e_Gene:
        loc_type = eSeq_nuc; // Can gene features have product?
        break;
    case CSeqFeatData::e_Cdregion:
        loc_type = eSeq_nuc;
        prod_type = eSeq_prot;
        break;
    case CSeqFeatData::e_Prot:
        loc_type = eSeq_prot; // Can protein features have product?
        break;
    case CSeqFeatData::e_Rna:
        loc_type = eSeq_nuc;
        prod_type = eSeq_nuc;
        break;
    /*
    case e_Org:
    case e_Pub:
    case e_Seq:
    case e_Imp:
    case e_Region:
    case e_Comment:
    case e_Bond:
    case e_Site:
    case e_Rsite:
    case e_User:
    case e_Txinit:
    case e_Num:
    case e_Psec_str:
    case e_Non_std_residue:
    case e_Het:
    case e_Biosrc:
    case e_Clone:
    */
    default:
        break;
    }

    if (loc_type != eSeq_unknown) {
        for (CSeq_loc_CI it(map_feat.GetLocation()); it; ++it) {
            CSeq_id_Handle idh = it.GetSeq_id_Handle();
            if (idh) {
                SetSeqTypeById(idh, loc_type);
            }
        }
    }
    if (prod_type != eSeq_unknown) {
        for (CSeq_loc_CI it(map_feat.GetProduct()); it; ++it) {
            CSeq_id_Handle idh = it.GetSeq_id_Handle();
            if (idh) {
                SetSeqTypeById(idh, prod_type);
            }
        }
    }

    int frame = 0;
    if (map_feat.GetData().IsCdregion()) {
        // For cd-regions use frame information.
        frame = map_feat.GetData().GetCdregion().GetFrame();
    }
    if (dir == eLocationToProduct) {
        x_InitializeLocs(map_feat.GetLocation(), map_feat.GetProduct(), frame);
    }
    else {
        x_InitializeLocs(map_feat.GetProduct(), map_feat.GetLocation(), frame);
    }
}


void CSeq_loc_Mapper_Base::x_InitializeLocs(const CSeq_loc& source,
                                            const CSeq_loc& target,
                                            int             frame)
{
    if (source.IsEmpty()  ||  target.IsEmpty()) {
        // Ignore mapping from or to an empty location.
        return;
    }

    // There are several passes - we need to find out sequence types
    // and lengths before creating the mappings.

    // First pass - collect sequence types (if possible) and
    // calculate total length of each location.
    TSeqPos src_total_len = 0; // total length of the source location
    TSeqPos dst_total_len = 0; // total length of the destination
    ESeqType src_type = eSeq_unknown; // source sequence type
    ESeqType dst_type = eSeq_unknown; // destination sequence type
    bool known_src_types = x_CheckSeqTypes(source, src_type, src_total_len);
    bool known_dst_types = x_CheckSeqTypes(target, dst_type, dst_total_len);
    // Check if all sequence types are known and there are no conflicts.
    bool known_types = known_src_types  &&  known_dst_types;
    if ( !known_types ) {
        // some types are still unknown, try other methods
        // First, if at least one sequence type is known, try to use it
        // for the whole location.
        // x_ForceSeqTypes will throw if there are different sequence types in
        // the same location.
        if (src_type == eSeq_unknown) {
            src_type = x_ForceSeqTypes(source);
        }
        if (dst_type == eSeq_unknown) {
            dst_type = x_ForceSeqTypes(target);
        }
        // If both source and destination types could be forced, don't
        // check sequence lengths.
        if (src_type == eSeq_unknown  ||  dst_type == eSeq_unknown) {
            // There are only unknown types in the source, destination
            // of both. Try to compare lengths of the locations.
            if (src_total_len == kInvalidSeqPos  ||
                dst_total_len == kInvalidSeqPos) {
                // Location lengths are unknown (e.g. whole seq-locs).
                // No way to create correct mappings.
                NCBI_THROW(CAnnotMapperException, eBadLocation,
                           "Undefined location length -- "
                           "unable to detect sequence type");
            }
            if (src_total_len == dst_total_len) {
                // If the lengths are the same, source and destination
                // have the same sequence type. If at least one of them
                // is known, use it for both.
                if (src_type != eSeq_unknown) {
                    dst_type = src_type;
                }
                else if (dst_type != eSeq_unknown) {
                    src_type = dst_type;
                }
                else if (frame) {
                    // Both sequence types are unknown, but frame is set.
                    // Assume they are proteins.
                    src_type = eSeq_prot;
                    dst_type = eSeq_prot;
                }
                // By default we assume that both sequences are nucleotides.
                // Even if it's a mapping between two proteins, this assumption
                // should work fine in most cases.
                // The only exception is when we try to map an alignment and
                // while parsing it we detect that the mapping was between
                // prots. In this case CSeq_align_Mapper_Base will call
                // x_AdjustSeqTypesToProt() to change the types and adjust
                // ranges according to the new sequence width.
            }
            // While checking if it's a mapping between nuc and prot,
            // truncate incomplete codons.
            else if (src_total_len/3 == dst_total_len) {
                if (src_type == eSeq_unknown) {
                    src_type = eSeq_nuc;
                }
                if (dst_type == eSeq_unknown) {
                    dst_type = eSeq_prot;
                }
                // Make sure there's no conflict between the known and
                // the calculated sequence types.
                if (src_type != eSeq_nuc  ||  dst_type != eSeq_prot) {
                    NCBI_THROW(CAnnotMapperException, eBadLocation,
                        "Sequence types (nuc to prot) are inconsistent with "
                        "location lengths");
                }
                // Report overhanging bases if any
                if (src_total_len % 3 != 0) {
                    ERR_POST_X(28, Warning <<
                        "Source and destination lengths do not match, "
                        "dropping " << src_total_len % 3 <<
                        " overhanging bases on source location");
                }
            }
            else if (dst_total_len/3 == src_total_len) {
                if (src_type == eSeq_unknown) {
                    src_type = eSeq_prot;
                }
                if (dst_type == eSeq_unknown) {
                    dst_type = eSeq_nuc;
                }
                // Make sure there's no conflict between the known and
                // the calculated sequence types.
                if (src_type != eSeq_prot  ||  dst_type != eSeq_nuc) {
                    NCBI_THROW(CAnnotMapperException, eBadLocation,
                        "Sequence types (prot to nuc) are inconsistent with "
                        "location lengths");
                }
                // Report overhanging bases if any
                if (dst_total_len % 3 != 0) {
                    ERR_POST_X(29, Warning <<
                        "Source and destination lengths do not match, "
                        "dropping " << dst_total_len % 3 <<
                        " overhanging bases on destination location");
                }
            }
            else {
                // If location lengths are not 1:1 or 1:3, there's no way
                // to get the right sequence types.
                NCBI_THROW(CAnnotMapperException, eBadLocation,
                           "Wrong location length -- "
                           "unable to detect sequence type");
            }
        }
    }
    // At this point all sequence types should be known or forced.
    // Set the widths.
    int src_width = (src_type == eSeq_prot) ? 3 : 1;
    int dst_width = (dst_type == eSeq_prot) ? 3 : 1;

    CSeq_loc_CI src_it(source, CSeq_loc_CI::eEmpty_Skip,
        CSeq_loc_CI::eOrder_Biological);
    CSeq_loc_CI dst_it(target, CSeq_loc_CI::eEmpty_Skip,
        CSeq_loc_CI::eOrder_Biological);

    // Get starts and lengths with care, check for empty and whole ranges.
    TRange rg = src_it.GetRange();
    // Start with an empty range
    TSeqPos src_start = kInvalidSeqPos;
    TSeqPos src_len = 0;
    if ( rg.IsWhole() ) {
        src_start = 0;
        src_len = kInvalidSeqPos;
    }
    else if ( !rg.Empty() ) {
        src_start = src_it.GetRange().GetFrom()*src_width;
        src_len = x_GetRangeLength(src_it)*src_width;
    }

    rg = dst_it.GetRange();
    TSeqPos dst_start = kInvalidSeqPos;
    TSeqPos dst_len = 0;
    if ( rg.IsWhole() ) {
        dst_start = 0;
        dst_len = kInvalidSeqPos;
    }
    else if ( !rg.Empty() ) {
        dst_start = dst_it.GetRange().GetFrom()*dst_width;
        dst_len = x_GetRangeLength(dst_it)*dst_width;
    }

    if ( frame ) {
        const int shift = frame - 1;
        if (dst_type == eSeq_prot  &&  src_start != kInvalidSeqPos &&
            static_cast<TSeqPos>(shift) < src_len ) {
            if( ! source.IsReverseStrand() ) {
                src_start += shift;
            }
            src_len -= shift;
        }
        if (src_type == eSeq_prot  &&  dst_start != kInvalidSeqPos &&
            static_cast<TSeqPos>(shift) < dst_len ) {
            if( ! target.IsReverseStrand() ) {
                dst_start += shift;
            }
            dst_len -= shift;
        }
    }
    // Iterate source and destination ranges.
    const TSeqPos src_bioseq_len = ( source.GetId() ? src_width * ( GetSequenceLength( *source.GetId() ) - 1 ) + (src_width - 1) : src_total_len );
    while (src_it  &&  dst_it) {
        // If sequence types were detected using lengths, set them now.
        if (src_type != eSeq_unknown) {
            SetSeqTypeById(src_it.GetSeq_id_Handle(), src_type);
        }
        if (dst_type != eSeq_unknown) {
            SetSeqTypeById(dst_it.GetSeq_id_Handle(), dst_type);
        }
        // Add new mapping range. This will adjust starts and lengths.
        x_NextMappingRange(
            src_it.GetSeq_id(), src_start, src_len, src_it.GetStrand(),
            dst_it.GetSeq_id(), dst_start, dst_len, dst_it.GetStrand(),
            dst_it.GetFuzzFrom(), dst_it.GetFuzzTo(), frame, dst_total_len, src_bioseq_len );
        // If the whole source or destination range was used, increment the
        // iterator.
        // This part may not work correctly if whole locations are
        // involved and lengths of the sequences can not be retrieved.
        // E.g. if the source contains 2 ranges and destination is a mix of
        // two whole locations (one per source range), dst_it will never be
        // incremented and both source ranges will be mapped to the same
        // sequence.
        if (src_len == 0  &&  ++src_it) {
            TRange rg = src_it.GetRange();
            if ( rg.Empty() ) {
                src_start = kInvalidSeqPos;
                src_len = 0;
            }
            else if ( rg.IsWhole() ) {
                src_start = 0;
                src_len = kInvalidSeqPos;
            }
            else {
                src_start = src_it.GetRange().GetFrom()*src_width;
                src_len = x_GetRangeLength(src_it)*src_width;
            }
        }
        if (dst_len == 0  &&  ++dst_it) {
            TRange rg = dst_it.GetRange();
            if ( rg.Empty() ) {
                dst_start = kInvalidSeqPos;
                dst_len = 0;
            }
            else if ( rg.IsWhole() ) {
                dst_start = 0;
                dst_len = kInvalidSeqPos;
            }
            else {
                dst_start = dst_it.GetRange().GetFrom()*dst_width;
                dst_len = x_GetRangeLength(dst_it)*dst_width;
            }
        }
    }
    // Remember the direction of source and destination. This information
    // will be used when ordering ranges in the mapped location.
    m_Mappings->SetReverseSrc(source.IsReverseStrand());
    m_Mappings->SetReverseDst(target.IsReverseStrand());
}


void CSeq_loc_Mapper_Base::x_InitializeAlign(const CSeq_align& map_align,
                                             const CSeq_id&    to_id)
{
    // When finding the destination row, the first row with required seq-id
    // is used. Do not check if there are multiple rows with the same id.
    switch ( map_align.GetSegs().Which() ) {
    case CSeq_align::C_Segs::e_Dendiag:
        {
            const TDendiag& diags = map_align.GetSegs().GetDendiag();
            ITERATE(TDendiag, diag_it, diags) {
                size_t to_row = size_t(-1);
                for (size_t i = 0; i < (*diag_it)->GetIds().size(); ++i) {
                    if ( (*diag_it)->GetIds()[i]->Equals(to_id) ) {
                        to_row = i;
                        break;
                    }
                }
                if (to_row == size_t(-1)) {
                    NCBI_THROW(CAnnotMapperException, eBadAlignment,
                               "Target ID not found in the alignment");
                }
                // Each diag forms a separate group. See SetMergeBySeg().
                m_CurrentGroup++;
                x_InitAlign(**diag_it, to_row);
            }
            break;
        }
    case CSeq_align::C_Segs::e_Denseg:
        {
            const CDense_seg& dseg = map_align.GetSegs().GetDenseg();
            size_t to_row = size_t(-1);
            for (size_t i = 0; i < dseg.GetIds().size(); ++i) {
                if (dseg.GetIds()[i]->Equals(to_id)) {
                    to_row = i;
                    break;
                }
            }
            if (to_row == size_t(-1)) {
                NCBI_THROW(CAnnotMapperException, eBadAlignment,
                           "Target ID not found in the alignment");
            }
            x_InitAlign(dseg, to_row);
            break;
        }
    case CSeq_align::C_Segs::e_Std:
        {
            const TStd& std_segs = map_align.GetSegs().GetStd();
            ITERATE(TStd, std_seg, std_segs) {
                size_t to_row = size_t(-1);
                if ((*std_seg)->IsSetIds()  &&  !(*std_seg)->GetIds().empty()) {
                    for (size_t i = 0; i < (*std_seg)->GetIds().size(); ++i) {
                        if ((*std_seg)->GetIds()[i]->Equals(to_id)) {
                            to_row = i;
                            break;
                        }
                    }
                }
                if (to_row == size_t(-1)) {
                    // The id is not found or 'ids' is missing in the std-seg.
                    // Try to parse seq-locs.
                    for (size_t i = 0; i < (*std_seg)->GetLoc().size(); ++i) {
                        const CSeq_id* row_id = (*std_seg)->GetLoc()[i]->GetId();
                        if (row_id  &&  row_id->Equals(to_id)) {
                            to_row = i;
                            break;
                        }
                    }
                }
                if (to_row == size_t(-1)) {
                    NCBI_THROW(CAnnotMapperException, eBadAlignment,
                               "Target ID not found in the alignment");
                }
                // Each std-seg forms a separate group. See SetMergeBySeg().
                m_CurrentGroup++;
                x_InitAlign(**std_seg, to_row);
            }
            break;
        }
    case CSeq_align::C_Segs::e_Packed:
        {
            const CPacked_seg& pseg = map_align.GetSegs().GetPacked();
            size_t to_row = size_t(-1);
            for (size_t i = 0; i < pseg.GetIds().size(); ++i) {
                if (pseg.GetIds()[i]->Equals(to_id)) {
                    to_row = i;
                    break;
                }
            }
            if (to_row == size_t(-1)) {
                NCBI_THROW(CAnnotMapperException, eBadAlignment,
                           "Target ID not found in the alignment");
            }
            x_InitAlign(pseg, to_row);
            break;
        }
    case CSeq_align::C_Segs::e_Disc:
        {
            const CSeq_align_set& aln_set = map_align.GetSegs().GetDisc();
            ITERATE(CSeq_align_set::Tdata, aln, aln_set.Get()) {
                // Each sub-alignment forms a separate group.
                // See SetMergeBySeg().
                m_CurrentGroup++;
                x_InitializeAlign(**aln, to_id);
            }
            break;
        }
    case CSeq_align::C_Segs::e_Spliced:
        {
            x_InitSpliced(map_align.GetSegs().GetSpliced(), to_id);
            break;
        }
    case CSeq_align::C_Segs::e_Sparse:
        {
            const CSparse_seg& sparse = map_align.GetSegs().GetSparse();
            size_t row = 0;
            ITERATE(CSparse_seg::TRows, it, sparse.GetRows()) {
                // Prefer to map from the second subrow to the first one
                // if their ids are the same.
                if ((*it)->GetFirst_id().Equals(to_id)) {
                    m_MapOptions &= ~fAlign_Sparse_ToSecond;
                    m_MapOptions |= fAlign_Sparse_ToFirst;
                }
                else if ((*it)->GetSecond_id().Equals(to_id)) {
                    m_MapOptions &= ~fAlign_Sparse_ToFirst;
                    m_MapOptions |= fAlign_Sparse_ToSecond;
                }
                x_InitSparse(sparse, row);
            }
            break;
        }
    default:
        NCBI_THROW(CAnnotMapperException, eBadAlignment,
                   "Unsupported alignment type");
    }
}


void CSeq_loc_Mapper_Base::x_InitializeAlign(const CSeq_align& map_align,
                                             size_t            to_row)
{
    switch ( map_align.GetSegs().Which() ) {
    case CSeq_align::C_Segs::e_Dendiag:
        {
            const TDendiag& diags = map_align.GetSegs().GetDendiag();
            ITERATE(TDendiag, diag_it, diags) {
                // Each diag forms a separate group. See SetMergeBySeg().
                m_CurrentGroup++;
                x_InitAlign(**diag_it, to_row);
            }
            break;
        }
    case CSeq_align::C_Segs::e_Denseg:
        {
            const CDense_seg& dseg = map_align.GetSegs().GetDenseg();
            x_InitAlign(dseg, to_row);
            break;
        }
    case CSeq_align::C_Segs::e_Std:
        {
            const TStd& std_segs = map_align.GetSegs().GetStd();
            ITERATE(TStd, std_seg, std_segs) {
                // Each std-seg forms a separate group. See SetMergeBySeg().
                m_CurrentGroup++;
                x_InitAlign(**std_seg, to_row);
            }
            break;
        }
    case CSeq_align::C_Segs::e_Packed:
        {
            const CPacked_seg& pseg = map_align.GetSegs().GetPacked();
            x_InitAlign(pseg, to_row);
            break;
        }
    case CSeq_align::C_Segs::e_Disc:
        {
            // Use the same row in each sub-alignment.
            const CSeq_align_set& aln_set = map_align.GetSegs().GetDisc();
            ITERATE(CSeq_align_set::Tdata, aln, aln_set.Get()) {
                // Each sub-alignment forms a separate group. See SetMergeBySeg().
                m_CurrentGroup++;
                x_InitializeAlign(**aln, to_row);
            }
            break;
        }
    case CSeq_align::C_Segs::e_Spliced:
        {
            // Spliced alignment row indexing is different, use enum
            // to avoid confusion.
            if (to_row == 0  ||  to_row == 1) {
                x_InitSpliced(map_align.GetSegs().GetSpliced(),
                    ESplicedRow(to_row));
            }
            else {
                NCBI_THROW(CAnnotMapperException, eBadAlignment,
                    "Invalid row number in spliced-seg alignment");
            }
            break;
        }
    case CSeq_align::C_Segs::e_Sparse:
        {
            x_InitSparse(map_align.GetSegs().GetSparse(), to_row);
            break;
        }
    default:
        NCBI_THROW(CAnnotMapperException, eBadAlignment,
                   "Unsupported alignment type");
    }
}


void CSeq_loc_Mapper_Base::x_InitAlign(const CDense_diag& diag, size_t to_row)
{
    // Check the alignment for consistency. Adjust invalid values, show
    // warnings if this happens.
    size_t dim = diag.GetDim();
    _ASSERT(to_row < dim);
    if (dim != diag.GetIds().size()) {
        ERR_POST_X(1, Warning << "Invalid 'ids' size in dendiag");
        dim = min(dim, diag.GetIds().size());
    }
    if (dim != diag.GetStarts().size()) {
        ERR_POST_X(2, Warning << "Invalid 'starts' size in dendiag");
        dim = min(dim, diag.GetStarts().size());
    }
    bool have_strands = diag.IsSetStrands();
    if (have_strands && dim != diag.GetStrands().size()) {
        ERR_POST_X(3, Warning << "Invalid 'strands' size in dendiag");
        dim = min(dim, diag.GetStrands().size());
    }

    ENa_strand dst_strand = have_strands ?
        diag.GetStrands()[to_row] : eNa_strand_unknown;
    const CSeq_id& dst_id = *diag.GetIds()[to_row];
    ESeqType dst_type = GetSeqTypeById(dst_id);
    int dst_width = (dst_type == eSeq_prot) ? 3 : 1;

    // In alignments with multiple sequence types segment length
    // should be multiplied by 3, while starts multiplier depends
    // on the sequence type.
    int len_width = 1;
    for (size_t row = 0; row < dim; ++row) {
        if (GetSeqTypeById(*diag.GetIds()[row]) == eSeq_prot) {
            len_width = 3;
            break;
        }
    }
    for (size_t row = 0; row < dim; ++row) {
        if (row == to_row) {
            continue;
        }
        const CSeq_id& src_id = *diag.GetIds()[row];
        ESeqType src_type = GetSeqTypeById(src_id);
        int src_width = (src_type == eSeq_prot) ? 3 : 1;
        TSeqPos src_len = diag.GetLen()*len_width;
        TSeqPos dst_len = src_len;
        TSeqPos src_start = diag.GetStarts()[row]*src_width;
        TSeqPos dst_start = diag.GetStarts()[to_row]*dst_width;
        ENa_strand src_strand = have_strands ?
            diag.GetStrands()[row] : eNa_strand_unknown;
        // Add mapping
        x_NextMappingRange(src_id, src_start, src_len, src_strand,
            dst_id, dst_start, dst_len, dst_strand, 0, 0);
        // Since the lengths are always the same, both source and
        // destination ranges must be used in one iteration.
        _ASSERT(!src_len  &&  !dst_len);
    }
}


void CSeq_loc_Mapper_Base::x_InitAlign(const CDense_seg& denseg,
                                       size_t to_row)
{
    // Check the alignment for consistency. Adjust invalid values, show
    // warnings if this happens.
    size_t dim = denseg.GetDim();
    _ASSERT(to_row < dim);

    size_t numseg = denseg.GetNumseg();
    // claimed dimension may not be accurate :-/
    if (numseg != denseg.GetLens().size()) {
        ERR_POST_X(4, Warning << "Invalid 'lens' size in denseg");
        numseg = min(numseg, denseg.GetLens().size());
    }
    if (dim != denseg.GetIds().size()) {
        ERR_POST_X(5, Warning << "Invalid 'ids' size in denseg");
        dim = min(dim, denseg.GetIds().size());
    }
    if (dim*numseg != denseg.GetStarts().size()) {
        ERR_POST_X(6, Warning << "Invalid 'starts' size in denseg");
        dim = min(dim*numseg, denseg.GetStarts().size()) / numseg;
    }
    bool have_strands = denseg.IsSetStrands();
    if (have_strands && dim*numseg != denseg.GetStrands().size()) {
        ERR_POST_X(7, Warning << "Invalid 'strands' size in denseg");
        dim = min(dim*numseg, denseg.GetStrands().size()) / numseg;
    }

    // In alignments with multiple sequence types segment length
    // should be multiplied by 3, while starts multiplier depends
    // on the sequence type.
    int len_width = 1;
    for (size_t row = 0; row < dim; ++row) {
        if (GetSeqTypeById(*denseg.GetIds()[row]) == eSeq_prot) {
            len_width = 3;
            break;
        }
    }

    const CSeq_id& dst_id = *denseg.GetIds()[to_row];
    ESeqType dst_type = GetSeqTypeById(dst_id);
    int dst_width = (dst_type == eSeq_prot) ? 3 : 1;
    for (size_t row = 0; row < dim; ++row) {
        if (row == to_row) {
            continue;
        }
        const CSeq_id& src_id = *denseg.GetIds()[row];

        ESeqType src_type = GetSeqTypeById(src_id);
        int src_width = (src_type == eSeq_prot) ? 3 : 1;

        // Depending on the flags we may need to use whole range
        // for each dense-seg ignoring its segments.
        if (m_MapOptions & fAlign_Dense_seg_TotalRange) {
            // Get total range for source and destination rows.
            // Both ranges must be not empty.
            TSeqRange r_src = denseg.GetSeqRange(row);
            TSeqRange r_dst = denseg.GetSeqRange(to_row);

            _ASSERT(r_src.GetLength() != 0  &&  r_dst.GetLength() != 0);
            ENa_strand dst_strand = have_strands ?
                denseg.GetStrands()[to_row] : eNa_strand_unknown;
            ENa_strand src_strand = have_strands ?
                denseg.GetStrands()[row] : eNa_strand_unknown;

            // Dense-seg can not contain whole ranges, no need to check the ranges.
            TSeqPos src_len = r_src.GetLength()*len_width;
            TSeqPos dst_len = r_dst.GetLength()*len_width;
            TSeqPos src_start = r_src.GetFrom()*src_width;
            TSeqPos dst_start = r_dst.GetFrom()*dst_width;

            if (src_len != dst_len) {
                ERR_POST_X(23, Error <<
                    "Genomic vs product length mismatch in dense-seg");
            }
            x_NextMappingRange(
                src_id, src_start, src_len, src_strand,
                dst_id, dst_start, dst_len, dst_strand,
                0, 0);
            // Since the lengths are always the same, both source and
            // destination ranges must be used in one iteration.
            if (src_len != 0  ||  dst_len != 0) {
                NCBI_THROW(CAnnotMapperException, eBadAlignment,
                    "Different lengths of source and destination rows "
                    "in dense-seg.");
            }
        } else {
            // Normal mode - use all segments instead of the total range.
            for (size_t seg = 0; seg < numseg; ++seg) {
                int i_src_start = denseg.GetStarts()[seg*dim + row];
                int i_dst_start = denseg.GetStarts()[seg*dim + to_row];
                if (i_src_start < 0  ||  i_dst_start < 0) {
                    // Ignore gaps
                    continue;
                }

                ENa_strand dst_strand = have_strands ?
                    denseg.GetStrands()[seg*dim + to_row] : eNa_strand_unknown;
                ENa_strand src_strand = have_strands ?
                    denseg.GetStrands()[seg*dim + row] : eNa_strand_unknown;

                TSeqPos src_len = denseg.GetLens()[seg]*len_width;
                TSeqPos dst_len = src_len;
                TSeqPos src_start = (TSeqPos)(i_src_start)*src_width;
                TSeqPos dst_start = (TSeqPos)(i_dst_start)*dst_width;
                x_NextMappingRange(src_id, src_start, src_len, src_strand,
                    dst_id, dst_start, dst_len, dst_strand, 0, 0);
                // Since the lengths are always the same, both source and
                // destination ranges must be used in one iteration.
                _ASSERT(!src_len  &&  !dst_len);
            }
        }
    }
}


void CSeq_loc_Mapper_Base::x_InitAlign(const CStd_seg& sseg, size_t to_row)
{
    // Check the alignment for consistency. Adjust invalid values, show
    // warnings if this happens.
    size_t dim = sseg.GetDim();
    if (dim != sseg.GetLoc().size()) {
        ERR_POST_X(8, Warning << "Invalid 'loc' size in std-seg");
        dim = min(dim, sseg.GetLoc().size());
    }
    if (sseg.IsSetIds()
        && dim != sseg.GetIds().size()) {
        ERR_POST_X(9, Warning << "Invalid 'ids' size in std-seg");
        dim = min(dim, sseg.GetIds().size());
    }

    const CSeq_loc& dst_loc = *sseg.GetLoc()[to_row];
    for (size_t row = 0; row < dim; ++row ) {
        if (row == to_row) {
            continue;
        }
        const CSeq_loc& src_loc = *sseg.GetLoc()[row];
        if ( src_loc.IsEmpty() ) {
            // skipped row in this segment
            continue;
        }
        // The mapping is just between two locations
        x_InitializeLocs(src_loc, dst_loc);
    }
}


void CSeq_loc_Mapper_Base::x_InitAlign(const CPacked_seg& pseg, size_t to_row)
{
    // Check the alignment for consistency. Adjust invalid values, show
    // warnings if this happens.
    size_t dim    = pseg.GetDim();
    size_t numseg = pseg.GetNumseg();
    // claimed dimension may not be accurate :-/
    if (numseg != pseg.GetLens().size()) {
        ERR_POST_X(10, Warning << "Invalid 'lens' size in packed-seg");
        numseg = min(numseg, pseg.GetLens().size());
    }
    if (dim != pseg.GetIds().size()) {
        ERR_POST_X(11, Warning << "Invalid 'ids' size in packed-seg");
        dim = min(dim, pseg.GetIds().size());
    }
    if (dim*numseg != pseg.GetStarts().size()) {
        ERR_POST_X(12, Warning << "Invalid 'starts' size in packed-seg");
        dim = min(dim*numseg, pseg.GetStarts().size()) / numseg;
    }
    bool have_strands = pseg.IsSetStrands();
    if (have_strands && dim*numseg != pseg.GetStrands().size()) {
        ERR_POST_X(13, Warning << "Invalid 'strands' size in packed-seg");
        dim = min(dim*numseg, pseg.GetStrands().size()) / numseg;
    }

    // In alignments with multiple sequence types segment length
    // should be multiplied by 3, while starts multiplier depends
    // on the sequence type.
    int len_width = 1;
    for (size_t row = 0; row < dim; ++row) {
        if (GetSeqTypeById(*pseg.GetIds()[row]) == eSeq_prot) {
            len_width = 3;
            break;
        }
    }

    const CSeq_id& dst_id = *pseg.GetIds()[to_row];
    ESeqType dst_type = GetSeqTypeById(dst_id);
    int dst_width = (dst_type == eSeq_prot) ? 3 : 1;

    for (size_t row = 0; row < dim; ++row) {
        if (row == to_row) {
            continue;
        }
        const CSeq_id& src_id = *pseg.GetIds()[row];
        ESeqType src_type = GetSeqTypeById(src_id);
        int src_width = (src_type == eSeq_prot) ? 3 : 1;
        for (size_t seg = 0; seg < numseg; ++seg) {
            if (!pseg.GetPresent()[seg*dim + row]  ||
                !pseg.GetPresent()[seg*dim + to_row]) {
                // Ignore gaps
                continue;
            }

            ENa_strand dst_strand = have_strands ?
                pseg.GetStrands()[seg*dim + to_row] : eNa_strand_unknown;
            ENa_strand src_strand = have_strands ?
                pseg.GetStrands()[seg*dim + row] : eNa_strand_unknown;

            TSeqPos src_len = pseg.GetLens()[seg]*len_width;
            TSeqPos dst_len = src_len;
            TSeqPos src_start = pseg.GetStarts()[seg*dim + row]*src_width;
            TSeqPos dst_start = pseg.GetStarts()[seg*dim + to_row]*dst_width;
            x_NextMappingRange(
                src_id, src_start, src_len, src_strand,
                dst_id, dst_start, dst_len, dst_strand,
                0, 0);
            // Since the lengths are always the same, both source and
            // destination ranges must be used in one iteration.
            _ASSERT(!src_len  &&  !dst_len);
        }
    }
}


void CSeq_loc_Mapper_Base::x_InitSpliced(const CSpliced_seg& spliced,
                                         const CSeq_id&      to_id)
{
    // For spliced-segs the default begavior should be to merge mapped
    // ranges by exon.
    SetMergeBySeg();
    // Assume the same seq-id can not be used in both genomic and product rows,
    // try find the correct row.
    if (spliced.IsSetGenomic_id()  &&  spliced.GetGenomic_id().Equals(to_id)) {
        x_InitSpliced(spliced, eSplicedRow_Gen);
        return;
    }
    if (spliced.IsSetProduct_id()  &&  spliced.GetProduct_id().Equals(to_id)) {
        x_InitSpliced(spliced, eSplicedRow_Prod);
        return;
    }
    // Global ids are not set or not equal to to_id, try to use per-exon ids.
    // Not sure if it's possible that per-exon ids are different from the
    // global ones, but if this happens let's just ignore the globals.
    // Another catch: the mapping destination will be the whole row rather
    // than only those exons, which contain the requested id.
    ITERATE(CSpliced_seg::TExons, it, spliced.GetExons()) {
        const CSpliced_exon& ex = **it;
        if (ex.IsSetGenomic_id()  &&  ex.GetGenomic_id().Equals(to_id)) {
            x_InitSpliced(spliced, eSplicedRow_Gen);
            return;
        }
        if (ex.IsSetProduct_id()  &&  ex.GetProduct_id().Equals(to_id)) {
            x_InitSpliced(spliced, eSplicedRow_Prod);
            return;
        }
    }
}


TSeqPos CSeq_loc_Mapper_Base::sx_GetExonPartLength(const CSpliced_exon_chunk& part)
{
    // Helper function - return exon part length regardless of its type.
    switch ( part.Which() ) {
    case CSpliced_exon_chunk::e_Match:
        return part.GetMatch();
    case CSpliced_exon_chunk::e_Mismatch:
        return part.GetMismatch();
    case CSpliced_exon_chunk::e_Diag:
        return part.GetDiag();
    case CSpliced_exon_chunk::e_Product_ins:
        return part.GetProduct_ins();
    case CSpliced_exon_chunk::e_Genomic_ins:
        return part.GetGenomic_ins();
    default:
        ERR_POST_X(22, Warning << "Unsupported CSpliced_exon_chunk type: " <<
            part.SelectionName(part.Which()) << ", ignoring the chunk.");
    }
    return 0;
}


void CSeq_loc_Mapper_Base::
x_AddExonPartsMapping(TSeqPos&        mapping_len,
                      ESplicedRow     to_row,
                      const CSeq_id&  gen_id,
                      TSeqPos&        gen_start,
                      TSeqPos&        gen_len,
                      ENa_strand      gen_strand,
                      const CSeq_id&  prod_id,
                      TSeqPos&        prod_start,
                      TSeqPos&        prod_len,
                      ENa_strand      prod_strand)
{
    if (mapping_len == 0) return;
    bool rev_gen = IsReverse(gen_strand);
    bool rev_prod = IsReverse(prod_strand);
    TSeqPos pgen_len = mapping_len;
    TSeqPos pprod_len = mapping_len;
    // Calculate starts depending on the strand.
    TSeqPos pgen_start = rev_gen ?
        gen_start + gen_len - mapping_len : gen_start;
    TSeqPos pprod_start = rev_prod ?
        prod_start + prod_len - mapping_len : prod_start;
    // Create the mapping.
    if (to_row == eSplicedRow_Prod) {
        x_NextMappingRange(
            gen_id, pgen_start, pgen_len, gen_strand,
            prod_id, pprod_start, pprod_len, prod_strand,
            0, 0);
    }
    else {
        x_NextMappingRange(
            prod_id, pprod_start, pprod_len, prod_strand,
            gen_id, pgen_start, pgen_len, gen_strand,
            0, 0);
    }
    // Since the lengths are always the same, both source and
    // destination ranges must be used in one iteration.
    _ASSERT(pgen_len == 0  && pprod_len == 0);
    if ( !rev_gen ) {
        gen_start += mapping_len;
    }
    gen_len -= mapping_len;
    if ( !rev_prod ) {
        prod_start += mapping_len;
    }
    prod_len -= mapping_len;
    mapping_len = 0;
}


void CSeq_loc_Mapper_Base::
x_IterateExonParts(const CSpliced_exon::TParts& parts,
                   ESplicedRow                  to_row,
                   const CSeq_id&               gen_id,
                   TSeqPos&                     gen_start,
                   TSeqPos&                     gen_len,
                   ENa_strand                   gen_strand,
                   const CSeq_id&               prod_id,
                   TSeqPos&                     prod_start,
                   TSeqPos&                     prod_len,
                   ENa_strand                   prod_strand)
{
    // Parse a single exon, create mapping for each part.
    bool rev_gen = IsReverse(gen_strand);
    bool rev_prod = IsReverse(prod_strand);
    // Merge parts participating in the mapping (match, mismatch, diag).
    // Calculate total length of the merged parts.
    TSeqPos mapping_len = 0;
    ITERATE(CSpliced_exon::TParts, it, parts) {
        const CSpliced_exon_chunk& part = **it;
        TSeqPos plen = sx_GetExonPartLength(part);
        // Only match, mismatch and diag are used for mapping.
        // Ignore insertions the same way as gaps in other alignment types.
        if ( part.IsMatch() || part.IsMismatch() || part.IsDiag() ) {
            mapping_len += plen;
            continue;
        }
        // Convert any collected ranges to a new mapping. Adjust starts and
        // lengths.
        x_AddExonPartsMapping(mapping_len, to_row,
            gen_id, gen_start, gen_len, gen_strand,
            prod_id, prod_start, prod_len, prod_strand);
        // Adjust starts and lengths to skip non-participating parts.
        if (!rev_gen  &&  !part.IsProduct_ins()) {
            gen_start += plen;
        }
        if (!rev_prod  &&  !part.IsGenomic_ins()) {
            prod_start += plen;
        }
        if ( !part.IsProduct_ins() ) {
            gen_len -= plen;
        }
        if ( !part.IsGenomic_ins() ) {
            prod_len -= plen;
        }
    }
    // Convert any remaining ranges to a new mapping. If mapping_len is zero,
    // nothing will be done.
    x_AddExonPartsMapping(mapping_len, to_row,
        gen_id, gen_start, gen_len, gen_strand,
        prod_id, prod_start, prod_len, prod_strand);
}


void CSeq_loc_Mapper_Base::x_InitSpliced(const CSpliced_seg& spliced,
                                         ESplicedRow         to_row)
{
    // Use global strands and seq-ids for all exons where no explicit
    // values are set.
    bool have_gen_strand = spliced.IsSetGenomic_strand();
    ENa_strand gen_strand = have_gen_strand ?
        spliced.GetGenomic_strand() : eNa_strand_unknown;
    bool have_prod_strand = spliced.IsSetProduct_strand();
    ENa_strand prod_strand = have_prod_strand ?
        spliced.GetProduct_strand() : eNa_strand_unknown;

    const CSeq_id* gen_id = spliced.IsSetGenomic_id() ?
        &spliced.GetGenomic_id() : 0;
    const CSeq_id* prod_id = spliced.IsSetProduct_id() ?
        &spliced.GetProduct_id() : 0;

    bool prod_is_prot = false;
    // Spliced-seg already contains the information about sequence types.
    switch ( spliced.GetProduct_type() ) {
    case CSpliced_seg::eProduct_type_protein:
        prod_is_prot = true;
        break;
    case CSpliced_seg::eProduct_type_transcript:
        // Leave both widths = 1
        break;
    default:
        ERR_POST_X(14, Error << "Unknown product type in spliced-seg");
        return;
    }

    ITERATE(CSpliced_seg::TExons, it, spliced.GetExons()) {
        // Use new group for each exon.
        m_CurrentGroup++;
        const CSpliced_exon& ex = **it;
        const CSeq_id* ex_gen_id = ex.IsSetGenomic_id() ?
            &ex.GetGenomic_id() : gen_id;
        const CSeq_id* ex_prod_id = ex.IsSetProduct_id() ?
            &ex.GetProduct_id() : prod_id;
        if (!ex_gen_id  ||  !ex_prod_id) {
            // No id is set globally or locally. Ignore the exon.
            ERR_POST_X(15, Error << "Missing id in spliced-exon");
            continue;
        }
        ENa_strand ex_gen_strand = ex.IsSetGenomic_strand() ?
            ex.GetGenomic_strand() : gen_strand;
        ENa_strand ex_prod_strand = ex.IsSetProduct_strand() ?
            ex.GetProduct_strand() : prod_strand;
        TSeqPos gen_from = ex.GetGenomic_start();
        TSeqPos gen_to = ex.GetGenomic_end();
        TSeqPos prod_from, prod_to;
        // Make sure coordinate types match product type.
        if (prod_is_prot != ex.GetProduct_start().IsProtpos()) {
            ERR_POST_X(24, Error <<
                "Wrong product-start type in spliced-exon, "
                "does not match product-type");
        }
        if (prod_is_prot != ex.GetProduct_end().IsProtpos()) {
            ERR_POST_X(25, Error <<
                "Wrong product-end type in spliced-exon, "
                "does not match product-type");
        }
        prod_from = ex.GetProduct_start().AsSeqPos();
        prod_to = ex.GetProduct_end().AsSeqPos();

        TSeqPos gen_len = gen_to - gen_from + 1;
        TSeqPos prod_len = prod_to - prod_from + 1;
        // Cache sequence type for the id.
        SetSeqTypeById(*ex_prod_id, prod_is_prot ? eSeq_prot : eSeq_nuc);
        SetSeqTypeById(*ex_gen_id, eSeq_nuc);
        if ( ex.IsSetParts() ) {
            // Iterate exon parts.
            x_IterateExonParts(ex.GetParts(), to_row,
               *ex_gen_id, gen_from, gen_len, ex_gen_strand,
               *ex_prod_id, prod_from, prod_len, ex_prod_strand);
        }
        else {
            // Use the whole exon if there are no parts.
            if ( to_row == eSplicedRow_Prod ) {
                x_NextMappingRange(
                    *ex_gen_id, gen_from, gen_len, ex_gen_strand,
                    *ex_prod_id, prod_from, prod_len, ex_prod_strand,
                    0, 0);
            }
            else {
                x_NextMappingRange(
                    *ex_prod_id, prod_from, prod_len, ex_prod_strand,
                    *ex_gen_id, gen_from, gen_len, ex_gen_strand,
                    0, 0);
            }
        }
        // Make sure the whole exon was used.
        if (gen_len  ||  prod_len) {
            ERR_POST_X(17, Error <<
                "Genomic vs product length mismatch in spliced-exon");
        }
    }
}


void CSeq_loc_Mapper_Base::x_InitSparse(const CSparse_seg& sparse,
                                        int to_row)
{
    // Sparse-seg needs special row indexing.
    bool to_second = (m_MapOptions & fAlign_Sparse_ToSecond) != 0;

    // Check the alignment for consistency. Adjust invalid values, show
    // warnings if this happens.
    _ASSERT(size_t(to_row) < sparse.GetRows().size());
    const CSparse_align& row = *sparse.GetRows()[to_row];

    size_t numseg = row.GetNumseg();
    // claimed dimension may not be accurate :-/
    if (numseg != row.GetFirst_starts().size()) {
        ERR_POST_X(18, Warning <<
            "Invalid 'first-starts' size in sparse-align");
        numseg = min(numseg, row.GetFirst_starts().size());
    }
    if (numseg != row.GetSecond_starts().size()) {
        ERR_POST_X(19, Warning <<
            "Invalid 'second-starts' size in sparse-align");
        numseg = min(numseg, row.GetSecond_starts().size());
    }
    if (numseg != row.GetLens().size()) {
        ERR_POST_X(20, Warning << "Invalid 'lens' size in sparse-align");
        numseg = min(numseg, row.GetLens().size());
    }
    bool have_strands = row.IsSetSecond_strands();
    if (have_strands  &&  numseg != row.GetSecond_strands().size()) {
        ERR_POST_X(21, Warning <<
            "Invalid 'second-strands' size in sparse-align");
        numseg = min(numseg, row.GetSecond_strands().size());
    }

    const CSeq_id& first_id = row.GetFirst_id();
    const CSeq_id& second_id = row.GetSecond_id();

    ESeqType first_type = GetSeqTypeById(first_id);
    ESeqType second_type = GetSeqTypeById(second_id);
    int first_width = (first_type == eSeq_prot) ? 3 : 1;
    int second_width = (second_type == eSeq_prot) ? 3 : 1;
    // In alignments with multiple sequence types segment length
    // should be multiplied by 3, while starts multiplier depends
    // on the sequence type.
    int len_width = (first_type == eSeq_prot  ||  second_type == eSeq_prot) ?
        3 : 1;
    const CSparse_align::TFirst_starts& first_starts = row.GetFirst_starts();
    const CSparse_align::TSecond_starts& second_starts = row.GetSecond_starts();
    const CSparse_align::TLens& lens = row.GetLens();
    const CSparse_align::TSecond_strands& strands = row.GetSecond_strands();

    // Iterate segments, create mapping for each segment.
    for (size_t i = 0; i < numseg; i++) {
        TSeqPos first_start = first_starts[i]*first_width;
        TSeqPos second_start = second_starts[i]*second_width;
        TSeqPos first_len = lens[i]*len_width;
        TSeqPos second_len = first_len;
        ENa_strand strand = have_strands ? strands[i] : eNa_strand_unknown;
        if ( to_second ) {
            x_NextMappingRange(
                first_id, first_start, first_len, eNa_strand_unknown,
                second_id, second_start, second_len, strand,
                0, 0);
        }
        else {
            x_NextMappingRange(
                second_id, second_start, second_len, strand,
                first_id, first_start, first_len, eNa_strand_unknown,
                0, 0);
        }
        // Make sure the whole segment was used.
        _ASSERT(!first_len  &&  !second_len);
    }
}


/////////////////////////////////////////////////////////////////////
//
//   Initialization helpers
//


const CSeq_id_Handle&
CSeq_loc_Mapper_Base::x_GetPrimaryId(const CSeq_id_Handle& synonym) const
{
    TSynonymMap::const_iterator primary_it = m_SynonymMap.find(synonym);
    return primary_it != m_SynonymMap.end() ? primary_it->second : synonym;
}


CSeq_loc_Mapper_Base::ESeqType
CSeq_loc_Mapper_Base::GetSeqType(const CSeq_id_Handle& idh) const
{
    // NOTE: Maping synonyms to the main id should be done by the caller.
    _ASSERT(m_SeqInfo);
    // Check cached types.
    TSeqTypeById::const_iterator found = m_SeqTypes.find(idh);
    if (found != m_SeqTypes.end()) {
        return found->second;
    }
    // New sequence - check the type and cache it.
    ESeqType seqtype = m_SeqInfo->GetSequenceType(idh);
    if (seqtype != eSeq_unknown) {
        // Cache sequence type for all synonyms if any
        SetSeqTypeById(idh, seqtype);
    }
    return seqtype;
}


void CSeq_loc_Mapper_Base::CollectSynonyms(const CSeq_id_Handle& id,
                                           TSynonyms& synonyms) const
{
    _ASSERT(m_SeqInfo);
    m_SeqInfo->CollectSynonyms(id, synonyms);
    if ( synonyms.empty() ) {
        synonyms.insert(id);
    }
}


const CSeq_id_Handle&
CSeq_loc_Mapper_Base::CollectSynonyms(const CSeq_id_Handle& id) const
{
    TSynonymMap::const_iterator primary_it = m_SynonymMap.find(id);
    if (primary_it != m_SynonymMap.end()) {
        return primary_it->second;
    }
    _ASSERT(m_SeqInfo);
    TSynonyms synonyms;
    m_SeqInfo->CollectSynonyms(id, synonyms);
    ITERATE(TSynonyms, syn, synonyms) {
        m_SynonymMap[*syn] = id;
    }
    return id;
}


TSeqPos CSeq_loc_Mapper_Base::GetSequenceLength(const CSeq_id& id)
{
    _ASSERT(m_SeqInfo);
    CSeq_id_Handle idh = CollectSynonyms(CSeq_id_Handle::GetHandle(id));
    TLengthMap::const_iterator it = m_LengthMap.find(idh);
    if (it != m_LengthMap.end()) {
        return it->second;
    }
    TSeqPos len = m_SeqInfo->GetSequenceLength(idh);
    m_LengthMap[idh] = len;
    return len;
}


void CSeq_loc_Mapper_Base::SetSeqTypeById(const CSeq_id_Handle& idh,
                                          ESeqType              seqtype) const
{
    // Do not store unknown types
    if (seqtype == eSeq_unknown) return;
    CSeq_id_Handle primary_id = CollectSynonyms(idh);
    TSeqTypeById::const_iterator it = m_SeqTypes.find(primary_id);
    if (it != m_SeqTypes.end()) {
        // If the type is already known and different from the new one,
        // throw the exception.
        if (it->second != seqtype) {
            NCBI_THROW(CAnnotMapperException, eOtherError,
                "Attempt to modify a known sequence type.");
        }
        return;
    }
    m_SeqTypes[primary_id] = seqtype;
}


bool CSeq_loc_Mapper_Base::x_CheckSeqTypes(const CSeq_loc& loc,
                                           ESeqType&       seqtype,
                                           TSeqPos&        len) const
{
    // Iterate the seq-loc, try to get sequence types used in it.
    len = 0;
    seqtype = eSeq_unknown;
    bool found_type = false;
    bool ret = true; // return true if types are known for all parts.
    for (CSeq_loc_CI it(loc); it; ++it) {
        CSeq_id_Handle idh = it.GetSeq_id_Handle();
        if ( !idh ) continue; // NULL?
        ESeqType it_type = GetSeqTypeById(idh);
        // Reset ret to false if there are unknown types.
        ret = ret && it_type != eSeq_unknown;
        if ( !found_type ) {
            seqtype = it_type;
            found_type = true;
        }
        else if (seqtype != it_type) {
            seqtype = eSeq_unknown; // Report multiple types as 'unknown'
        }
        // Adjust total length or reset it.
        if (len != kInvalidSeqPos) {
            if ( it.GetRange().IsWhole() ) {
                len = kInvalidSeqPos;
            }
            else {
                len += it.GetRange().GetLength();
            }
        }
    }
    return ret;
}


CSeq_loc_Mapper_Base::ESeqType
CSeq_loc_Mapper_Base::x_ForceSeqTypes(const CSeq_loc& loc) const
{
    // Try to find at least one known sequence type and use it for
    // all unknown parts.
    ESeqType ret = eSeq_unknown;
    set<CSeq_id_Handle> handles; // Collect all seq-ids used in the location
    for (CSeq_loc_CI it(loc); it; ++it) {
        CSeq_id_Handle idh = it.GetSeq_id_Handle();
        if ( !idh ) continue; // NULL?
        idh = CollectSynonyms(idh);
        TSeqTypeById::iterator st = m_SeqTypes.find(idh);
        if (st != m_SeqTypes.end()  &&  st->second != eSeq_unknown) {
            // New sequence type could be detected.
            if (ret == eSeq_unknown) {
                ret = st->second; // Remember the type if not set yet.
            }
            else if (ret != st->second) {
                // There are different types in the location and some are
                // unknown - impossible to use this for mapping.
                NCBI_THROW(CAnnotMapperException, eBadLocation,
                    "Unable to detect sequence types in the locations.");
            }
        }
        handles.insert(idh); // Store the new id
    }
    if (ret != eSeq_unknown) {
        // At least some types could be detected and there were no conflicts.
        // Use the found type for all other ranges.
        ITERATE(set<CSeq_id_Handle>, it, handles) {
            m_SeqTypes[*it] = ret;
        }
    }
    return ret; // Return the found type or unknown.
}


void CSeq_loc_Mapper_Base::x_AdjustSeqTypesToProt(const CSeq_id_Handle& idh)
{
    // The function is used when seq-align mapper suddenly detects that
    // sequence types were incorrectly set to nuc during the initialization.
    // This is possible only when the mapping is from a protein to a protein,
    // the scope (or other source of type information) is not available, and
    // the seq-loc mapper decides the mapping is from nuc to nuc. The seq-align
    // mapper may deduce the real sequence types from the alignment to be
    // mapped. In this case it will ask the seq-loc mapper to adjust types.
    // We need to check a lot of conditions not to spoil the mapping data.
    bool have_id = false;    // Is the id known to the mapper?
    bool have_known = false; // Are there known sequence types?
    CSeq_id_Handle primary_id = x_GetPrimaryId(idh);
    // Make sure all ids have unknown types (could not be detected during
    // the initialization).
    ITERATE(CMappingRanges::TIdMap, id_it, m_Mappings->GetIdMap()) {
        if (id_it->first == primary_id) {
            have_id = true;
        }
        if (GetSeqTypeById(id_it->first) != eSeq_unknown) {
            have_known = true;
        }
    }
    // The requested id is not used in the mappings - ignore the request.
    if ( !have_id ) return;
    if ( have_known ) {
        // Some sequence types are already known, we can not adjust anything.
        NCBI_THROW(CAnnotMapperException, eOtherError,
            "Can not adjust sequence types to protein.");
    }
    // Now we have to copy all the mappings adjusting there sequence types
    // and coordinates.
    CRef<CMappingRanges> old_mappings = m_Mappings;
    m_Mappings.Reset(new CMappingRanges);
    ITERATE(CMappingRanges::TIdMap, id_it, old_mappings->GetIdMap()) {
        SetSeqTypeById(id_it->first, eSeq_prot);
        // Adjust all starts and lengths
        ITERATE(CMappingRanges::TRangeMap, rg_it, id_it->second) {
            const CMappingRange& mrg = *rg_it->second;
            TSeqPos src_from = mrg.m_Src_from;
            if (src_from != kInvalidSeqPos) src_from *= 3;
            TSeqPos dst_from = mrg.m_Dst_from;
            if (dst_from != kInvalidSeqPos) dst_from *= 3;
            TSeqPos len = mrg.m_Src_to - mrg.m_Src_from + 1;
            if (len != kInvalidSeqPos) len *= 3;
            CRef<CMappingRange> new_rg = m_Mappings->AddConversion(
                mrg.m_Src_id_Handle, src_from, len, mrg.m_Src_strand,
                mrg.m_Dst_id_Handle, dst_from, mrg.m_Dst_strand,
                mrg.m_ExtTo);
            new_rg->SetGroup(mrg.GetGroup());
        }
    }
    // Also update m_DstRanges. They must also use genomic coordinates.
    NON_CONST_ITERATE(TDstStrandMap, str_it, m_DstRanges) {
        NON_CONST_ITERATE(TDstIdMap, id_it, *str_it) {
            NON_CONST_ITERATE(TDstRanges, rg_it, id_it->second) {
                TSeqPos from = kInvalidSeqPos;
                TSeqPos to = 0;
                if ( rg_it->IsWhole() ) {
                    from = 0;
                    to = kInvalidSeqPos;
                }
                else if ( !rg_it->Empty() ) {
                    from = rg_it->GetFrom()*3;
                    to = rg_it->GetToOpen()*3;
                }
                rg_it->SetOpen(from, to);
            }
        }
    }
}


TSeqPos CSeq_loc_Mapper_Base::x_GetRangeLength(const CSeq_loc_CI& it)
{
    if (it.IsWhole()  &&  IsReverse(it.GetStrand())) {
        // This should not happen since whole locations do not have strands.
        // But just for the safety we need real interval length for minus
        // strand not "whole", to calculate mapping coordinates.
        // This can also fail. There are some additional checks in the
        // calling function (see x_InitializeLocs).
        return GetSequenceLength(it.GetSeq_id());
    }
    else {
        return it.GetRange().GetLength();
    }
}


void CSeq_loc_Mapper_Base::x_NextMappingRange(const CSeq_id&   src_id,
                                              TSeqPos&         src_start,
                                              TSeqPos&         src_len,
                                              ENa_strand       src_strand,
                                              const CSeq_id&   dst_id,
                                              TSeqPos&         dst_start,
                                              TSeqPos&         dst_len,
                                              ENa_strand       dst_strand,
                                              const CInt_fuzz* fuzz_from,
                                              const CInt_fuzz* fuzz_to,
                                              int              frame,
                                              TSeqPos          dst_total_len,
                                              TSeqPos          src_bioseq_len )
{
    TSeqPos cvt_src_start = src_start;
    TSeqPos cvt_dst_start = dst_start;
    TSeqPos cvt_length;

    const TSeqPos original_dst_len = dst_len;

    if (src_len == dst_len) {
        if (src_len == kInvalidSeqPos) {
            // Mapping whole to whole - try to get real length.
            src_len = GetSequenceLength(src_id);
            if (src_len != kInvalidSeqPos) {
                src_len -= src_start;
            }
            dst_len = GetSequenceLength(dst_id);
            if (dst_len != kInvalidSeqPos) {
                dst_len -= dst_start;
            }
            // GetSequenceLength() could fail to get the real length.
            // We can still try to initialize the mapper but with care.
            // If a location is whole, its start must be 0 and strand unknown.
            _ASSERT(src_len != kInvalidSeqPos  ||
                (src_start == 0  &&  src_strand == eNa_strand_unknown));
            _ASSERT(dst_len != kInvalidSeqPos  ||
                (dst_start == 0  &&  dst_strand == eNa_strand_unknown));
        }
        cvt_length = src_len;
        src_len = 0;
        dst_len = 0;
    }
    else if (src_len > dst_len) {
        // It is possible that the source location is whole. In this
        // case its strand must be not set.
        _ASSERT(src_len != kInvalidSeqPos || src_strand == eNa_strand_unknown);
        // Destination range is shorter - use it as a single interval,
        // adjust source range according to its strand.
        if (IsReverse(src_strand)) {
            cvt_src_start += src_len - dst_len;
        }
        else {
            src_start += dst_len;
        }
        cvt_length = dst_len;
        // Do not adjust length of a whole location.
        if (src_len != kInvalidSeqPos) {
            src_len -= cvt_length;
        }
        dst_len = 0; // Destination has been used completely.
    }
    else { // if (src_len < dst_len)
        // It is possible that the destination location is whole. In this
        // case its strand must be not set.
        _ASSERT(dst_len != kInvalidSeqPos || dst_strand == eNa_strand_unknown);
        // Source range is shorter - use it as a single interval,
        // adjust destination range according to its strand.
        if ( IsReverse(dst_strand) ) {
            cvt_dst_start += dst_len - src_len;
        }
        else {
            dst_start += src_len;
        }
        cvt_length = src_len;
        // Do not adjust length of a whole location.
        if (dst_len != kInvalidSeqPos) {
            dst_len -= cvt_length;
        }
        src_len = 0; // Source has been used completely.
    }
    // Special case: prepare to extend mapped "to" if:
    // - mapping is from prot to nuc
    // - destination "to" is partial.
    // See also CMappingRange::m_ExtTo
    bool ext_to = false;
    ESeqType src_type = GetSeqTypeById(src_id);
    ESeqType dst_type = GetSeqTypeById(dst_id);
    if (src_type == eSeq_prot  &&  dst_type == eSeq_nuc) {
        if ( IsReverse(dst_strand) && fuzz_from ) {
            ext_to = fuzz_from  &&
                fuzz_from->IsLim()  &&
                fuzz_from->GetLim() == CInt_fuzz::eLim_lt;
        }
        else if ( !IsReverse(dst_strand) && fuzz_to ) {
            ext_to = fuzz_to  &&
                fuzz_to->IsLim()  &&
                fuzz_to->GetLim() == CInt_fuzz::eLim_gt;
        }
    }
    // Ready to add the conversion.
    x_AddConversion(src_id, cvt_src_start, src_strand,
        dst_id, cvt_dst_start, dst_strand, cvt_length, ext_to, frame, 
        dst_total_len, src_bioseq_len, original_dst_len );
}


void CSeq_loc_Mapper_Base::x_AddConversion(const CSeq_id& src_id,
                                           TSeqPos        src_start,
                                           ENa_strand     src_strand,
                                           const CSeq_id& dst_id,
                                           TSeqPos        dst_start,
                                           ENa_strand     dst_strand,
                                           TSeqPos        length,
                                           bool           ext_right,
                                           int            frame,
                                           TSeqPos        dst_total_len,
                                           TSeqPos        src_bioseq_len,
                                           TSeqPos        dst_len)
{
    // Make sure the destination ranges for the strand do exist.
    if (m_DstRanges.size() <= size_t(dst_strand)) {
        m_DstRanges.resize(size_t(dst_strand) + 1);
    }
    CSeq_id_Handle main_id = CollectSynonyms(CSeq_id_Handle::GetHandle(src_id));
    CRef<CMappingRange> rg = m_Mappings->AddConversion(
        main_id, src_start, length, src_strand,
        CSeq_id_Handle::GetHandle(dst_id), dst_start, dst_strand,
        ext_right, frame, dst_total_len, src_bioseq_len, dst_len );
    if ( m_CurrentGroup ) {
        rg->SetGroup(m_CurrentGroup);
    }
    // Add destination range.
    m_DstRanges[size_t(dst_strand)][CSeq_id_Handle::GetHandle(dst_id)]
        .push_back(TRange(dst_start, dst_start + length - 1));
}


void CSeq_loc_Mapper_Base::x_PreserveDestinationLocs(void)
{
    // Iterate destination ranges and create dummy mappings from
    // destination to destination so than ranges already on the
    // target sequence are not lost. This function is used only
    // when mapping between a sequence and its parts (through
    // a bioseq handle or a seq-map).
    for (size_t str_idx = 0; str_idx < m_DstRanges.size(); str_idx++) {
        NON_CONST_ITERATE(TDstIdMap, id_it, m_DstRanges[str_idx]) {
            CSeq_id_Handle main_id = CollectSynonyms(id_it->first);
            // Sort the ranges so that they can be merged.
            id_it->second.sort();
            TSeqPos dst_start = kInvalidSeqPos;
            TSeqPos dst_stop = kInvalidSeqPos;
            ESeqType dst_type = GetSeqTypeById(id_it->first);
            int dst_width = (dst_type == eSeq_prot) ? 3 : 1;
            ITERATE(TDstRanges, rg_it, id_it->second) {
                // Collect and merge ranges
                TSeqPos rg_start = kInvalidSeqPos;
                TSeqPos rg_stop = 0;
                if ( rg_it->IsWhole() ) {
                    rg_start = 0;
                    rg_stop = kInvalidSeqPos;
                }
                else if ( !rg_it->Empty() ) {
                    rg_start = rg_it->GetFrom()*dst_width;
                    rg_stop = rg_it->GetTo()*dst_width;
                }
                // The following will also be true if the first destination
                // range is empty. Ignore it anyway.
                if (dst_start == kInvalidSeqPos) {
                    dst_start = rg_start;
                    dst_stop = rg_stop;
                    continue;
                }
                if (dst_stop != kInvalidSeqPos  &&  rg_start <= dst_stop + 1) {
                    // overlapping or abutting ranges, continue collecting
                    dst_stop = max(dst_stop, rg_stop);
                    continue;
                }
                // Separate ranges, add conversion and restart collecting
                m_Mappings->AddConversion(
                    main_id, dst_start,
                    dst_stop == kInvalidSeqPos
                    ? kInvalidSeqPos : dst_stop - dst_start + 1,
                    ENa_strand(str_idx),
                    id_it->first, dst_start, ENa_strand(str_idx));
                // Do we have the whole sequence already?
                if (dst_stop == kInvalidSeqPos) {
                    // Prevent the range to be added one more time.
                    dst_start = dst_stop;
                    break;
                }
                // Proceed to the next range.
                dst_start = rg_start;
                dst_stop = rg_stop;
            }
            // Add any remaining range.
            if (dst_start < dst_stop) {
                m_Mappings->AddConversion(
                    main_id, dst_start,
                    dst_stop == kInvalidSeqPos
                    ? kInvalidSeqPos : dst_stop - dst_start + 1,
                    ENa_strand(str_idx),
                    id_it->first, dst_start, ENa_strand(str_idx));
            }
        }
    }
    m_DstRanges.clear();
}


/////////////////////////////////////////////////////////////////////
//
//   Mapping methods
//

void CSeq_loc_Mapper_Base::x_StripExtraneousFuzz(CRef<CSeq_loc>& loc) const
{
    if( loc ) {
        CRef<CSeq_loc> new_loc( new CSeq_loc );
        bool is_first = true;
        const ESeqLocExtremes extreme = eExtreme_Biological;

        CSeq_loc_CI loc_iter( *loc, CSeq_loc_CI::eEmpty_Allow );
        for( ; loc_iter; ++loc_iter ) {
            CConstRef<CSeq_loc> loc_piece( loc_iter.GetRangeAsSeq_loc() );

            // remove nonsense (to C) fuzz like "range fuzz" from result
            loc_piece = x_FixNonsenseFuzz(loc_piece);
            
            if( loc_piece && ( loc_piece->IsPartialStart(extreme) || loc_piece->IsPartialStop(extreme) ) ) {
                const bool is_last = ( ++CSeq_loc_CI(loc_iter) == loc->end() );

                CRef<CSeq_loc> new_loc_piece( new CSeq_loc );
                new_loc_piece->Assign( *loc_piece );

                if( ! is_first ) {
                  new_loc_piece->SetPartialStart( false, extreme ) ;
                }
                if( ! is_last ) {
                  new_loc_piece->SetPartialStop( false, extreme );
                }
                
                new_loc->Add( *new_loc_piece );
            } else {
                new_loc->Add( *loc_piece );
            }

            is_first = false;
        }

        loc = new_loc;
    }
}

CConstRef<CSeq_loc> 
CSeq_loc_Mapper_Base::x_FixNonsenseFuzz( 
    CConstRef<CSeq_loc> loc_piece ) const
{
    switch( loc_piece->Which() ) {
    case CSeq_loc::e_Int:
        {
            const CSeq_interval &seq_int = loc_piece->GetInt();

            const bool from_fuzz_is_bad = 
                ( seq_int.IsSetFuzz_from() && 
                    ( seq_int.GetFuzz_from().IsRange() || 
                        (seq_int.GetFuzz_from().IsLim() && 
                            seq_int.GetFuzz_from().GetLim() == CInt_fuzz::eLim_gt ) ) );
            const bool to_fuzz_is_bad = 
                ( seq_int.IsSetFuzz_to() && 
                    ( seq_int.GetFuzz_to().IsRange() || 
                        (seq_int.GetFuzz_to().IsLim() && 
                            seq_int.GetFuzz_to().GetLim() == CInt_fuzz::eLim_lt ) ) );

            if( from_fuzz_is_bad || to_fuzz_is_bad ) {
                CRef<CSeq_loc> new_loc( new CSeq_loc );
                new_loc->Assign( *loc_piece );

                if( from_fuzz_is_bad ) {
                    new_loc->SetInt().ResetFuzz_from();
                }

                if( to_fuzz_is_bad ) {
                    new_loc->SetInt().ResetFuzz_to();
                }

                return new_loc;
            }
        }
        break;
    case CSeq_loc::e_Pnt:
        {
            const CSeq_point &pnt = loc_piece->GetPnt();

            const bool is_fuzz_range =
                ( pnt.IsSetFuzz() && pnt.GetFuzz().IsRange() );
            if( is_fuzz_range ) {
                CRef<CSeq_loc> new_loc( new CSeq_loc );
                new_loc->Assign( *loc_piece );

                new_loc->SetPnt().ResetFuzz();

                return new_loc;
            }
        }
        break;
    default:
        break;
    }

    // the vast majority of the time we should end up here
    return loc_piece;
}

// Check location type, optimize if possible (empty mix to NULL,
// mix with a single element to this element etc.).
void CSeq_loc_Mapper_Base::x_OptimizeSeq_loc(CRef<CSeq_loc>& loc) const
{
    if ( !loc ) {
        loc.Reset(new CSeq_loc);
        loc->SetNull();
        return;
    }
    switch (loc->Which()) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Feat:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
    case CSeq_loc::e_Whole:
    case CSeq_loc::e_Int:
    case CSeq_loc::e_Pnt:
    case CSeq_loc::e_Equiv:
    case CSeq_loc::e_Bond:
    case CSeq_loc::e_Packed_int:
    case CSeq_loc::e_Packed_pnt:
        return;
    case CSeq_loc::e_Mix:
        {
            // remove final NULL, if any
            {{
                CSeq_loc_mix::Tdata &mix_locs = loc->SetMix().Set();
                while( (mix_locs.size() > 1) && 
                       (mix_locs.back()->IsNull()) ) 
                {
                    mix_locs.pop_back();
                }
            }}

            switch ( loc->GetMix().Get().size() ) {
            case 0:
                // Empty mix - convert to Null.
                loc->SetNull();
                break;
            case 1:
                {
                    // Mix with a single element - propagate it to the
                    // top level.
                    CRef<CSeq_loc> single = *loc->SetMix().Set().begin();
                    loc = single;
                    break;
                }
            default:
                {
                    // Try to convert to packed-int
                    CRef<CSeq_loc> packed;
                    NON_CONST_ITERATE(CSeq_loc_mix::Tdata, it,
                        loc->SetMix().Set()) {
                        // If there is something other than int, stop the
                        // optimization and leave the mix as-is.
                        if ( !(*it)->IsInt() ) {
                            packed.Reset();
                            break;
                        }
                        if ( !packed ) {
                            packed.Reset(new CSeq_loc);
                        }
                        packed->SetPacked_int().Set().
                            push_back(Ref(&(*it)->SetInt()));
                    }
                    if ( packed ) {
                        loc = packed;
                    }
                    break;
                }
            }
            break;
        }
    default:
        NCBI_THROW(CAnnotMapperException, eBadLocation,
                   "Unsupported location type");
    }
}


// Map a single range. Use mappings[cvt_idx] for mapping.
// last_src_to indicates were the previous mapping has ended (this may
// be left or right end depending on the source strand).
// For the first mapping last_src_to must be set to kInvalidSeqPos.
bool CSeq_loc_Mapper_Base::x_MapNextRange(const TRange&     src_rg,
                                          bool              is_set_strand,
                                          ENa_strand        src_strand,
                                          const TRangeFuzz& src_fuzz,
                                          TSortedMappings&  mappings,
                                          size_t            cvt_idx,
                                          TSeqPos*          last_src_to)
{
    const CMappingRange& cvt = *mappings[cvt_idx];
    if ( !cvt.CanMap(src_rg.GetFrom(), src_rg.GetTo(),
        is_set_strand && m_CheckStrand, src_strand) ) {
        // Can not map the range through this mapping.
        return false;
    }
    // The source range should be already using genomic coords.
    TSeqPos left = src_rg.GetFrom();
    TSeqPos right = src_rg.GetTo();
    bool partial_left = false;
    bool partial_right = false;
    // Used source sub-range is required to adjust graph data.
    // The values are relative to the source range.
    TRange used_rg = (src_rg.IsWhole() || src_rg.Empty()) ? src_rg :
        TRange(0, src_rg.GetLength() - 1);

    bool reverse = IsReverse(src_strand);

    // Check if the source range is truncated by the mapping.
    if (left < cvt.m_Src_from) {
        used_rg.SetFrom(cvt.m_Src_from - left);
        left = cvt.m_Src_from;
        if ( !reverse ) {
            // Partial if there's a gap between left and last_src_to.
            partial_left = (*last_src_to == kInvalidSeqPos)  ||
                (left != *last_src_to + 1);
        }
        else {
            // Partial if there's gap between left and next cvt. right end.
            partial_left = (cvt_idx == mappings.size() - 1)  ||
                (mappings[cvt_idx + 1]->m_Src_to + 1 != left);
        }
    }
    if (right > cvt.m_Src_to) {
        used_rg.SetLength(cvt.m_Src_to - left + 1);
        right = cvt.m_Src_to;
        if ( !reverse ) {
            // Partial if there's gap between right and next cvt. left end.
            partial_right = (cvt_idx == mappings.size() - 1)  ||
                (mappings[cvt_idx + 1]->m_Src_from != right + 1);
        }
        else {
            // Partial if there's gap between right and last_src_to.
            partial_right = (*last_src_to == kInvalidSeqPos)  ||
                (right + 1 != *last_src_to);
        }
    }
    if (right < left) {
        // Empty range - ignore it.
        return false;
    }
    // Adjust last mapped range end.
    *last_src_to = reverse ? left : right;

    TRangeFuzz fuzz;

    if( (m_FuzzOption & fFuzzOption_CStyle) == 0 ) {
        //// Indicate partial ranges using fuzz.
        if ( partial_left ) {
            // Set fuzz-from if a range was skipped on the left.
            fuzz.first.Reset(new CInt_fuzz);
            fuzz.first->SetLim(CInt_fuzz::eLim_lt);
        }
        else {
            if ( (!reverse  &&  cvt_idx == 0)  ||
                (reverse  &&  cvt_idx == mappings.size() - 1) ) {
                    // Preserve fuzz-from on the left end if any.
                    fuzz.first = src_fuzz.first;
            }
        }
        if ( partial_right ) {
            // Set fuzz-to if a range will be skipped on the right.
            fuzz.second.Reset(new CInt_fuzz);
            fuzz.second->SetLim(CInt_fuzz::eLim_gt);
        }
        else {
            if ( (reverse  &&  cvt_idx == 0)  ||
                (!reverse  &&  cvt_idx == mappings.size() - 1) ) {
                    // Preserve fuzz-to on the right end if any.
                    fuzz.second = src_fuzz.second;
            }
        }
    } else {
        fuzz = src_fuzz;
    }
    // If the previous range could not be mapped and was removed,
    // indicate it using fuzz.
    if ( m_LastTruncated ) {
        // TODO: Reconsider this "if" after we switch permanently to C++
        if ( ((m_FuzzOption & fFuzzOption_CStyle) == 0) && !fuzz.first ) {
            if( (m_FuzzOption & fFuzzOption_RemoveLimTlOrTr) != 0 ) {
                // we set lt or gt, as appropriate for strand
                if (reverse && !fuzz.second) {
                    fuzz.second.Reset(new CInt_fuzz);
                    fuzz.second->SetLim(CInt_fuzz::eLim_gt);
                }
                else if (!reverse && !fuzz.first) {
                    fuzz.first.Reset(new CInt_fuzz);
                    fuzz.first->SetLim(CInt_fuzz::eLim_lt);
                }
            } else {
                // Set fuzz for the original location.
                // This may be reversed later while mapping.
                if ( !reverse ) {
                    fuzz.first.Reset(new CInt_fuzz);
                    fuzz.first->SetLim(CInt_fuzz::eLim_tl);
                }
                else {
                    fuzz.second.Reset(new CInt_fuzz);
                    fuzz.second->SetLim(CInt_fuzz::eLim_tr);
                }
            }
        }
        // Reset the flag - current range is mapped at least partially.
        m_LastTruncated = false;
    }

    // Map fuzz to the destination. This will also adjust fuzz lim value
    // (just set by truncation) when strand is reversed by the mapping.
    TRangeFuzz mapped_fuzz = cvt.Map_Fuzz(fuzz);

    // Map the range and the strand. Fuzz is required to extend mapped
    // range in case of cd-region - see CMappingRange::m_ExtTo.
    TRange rg = cvt.Map_Range(left, right, &src_fuzz);
    ENa_strand dst_strand;
    bool is_set_dst_strand = cvt.Map_Strand(is_set_strand,
        src_strand, &dst_strand);
    // Store the new mapped range and its source.
    x_PushMappedRange(cvt.m_Dst_id_Handle,
                      STRAND_TO_INDEX(is_set_dst_strand, dst_strand),
                      rg, mapped_fuzz, cvt.m_Reverse, cvt.m_Group);
    x_PushSourceRange(cvt.m_Src_id_Handle,
        STRAND_TO_INDEX(is_set_strand, src_strand),
        STRAND_TO_INDEX(is_set_dst_strand, dst_strand),
        TRange(left, right), cvt.m_Reverse);
    // If mapping a graph, store the information required to adjust its data.
    if ( m_GraphRanges  &&  !used_rg.Empty() ) {
        m_GraphRanges->AddRange(used_rg);
        if ( !src_rg.IsWhole() ) {
            m_GraphRanges->IncOffset(src_rg.GetLength());
        }
    }
    return true;
}


void CSeq_loc_Mapper_Base::x_SetLastTruncated(void)
{
    // The flag indicates if the last range could not be mapped
    // or preserved and was dropped.
    if ( m_LastTruncated  ||  m_KeepNonmapping ) {
        return;
    }
    m_LastTruncated = true;
    // Update the mapped location before checking its properties.
    x_PushRangesToDstMix();
    // If the mapped location does not have any fuzz set, set it to
    // indicate the truncated part.
    if ( m_Dst_loc  &&  !m_Dst_loc->IsPartialStop(eExtreme_Biological) ) {
        if( (m_FuzzOption & fFuzzOption_RemoveLimTlOrTr) == 0 ) {
            m_Dst_loc->SetTruncatedStop(true, eExtreme_Biological);
        }
    }
}


// Map a single interval. Return true if the range could be mapped
// at least partially.
bool CSeq_loc_Mapper_Base::x_MapInterval(const CSeq_id&   src_id,
                                         TRange           src_rg,
                                         bool             is_set_strand,
                                         ENa_strand       src_strand,
                                         TRangeFuzz       orig_fuzz)
{
    bool res = false;
    CSeq_id_Handle src_idh = x_GetPrimaryId(CSeq_id_Handle::GetHandle(src_id));
    ESeqType src_type = GetSeqTypeById(src_idh);
    if (src_type == eSeq_prot  &&  !(src_rg.IsWhole() || src_rg.Empty()) ) {
        src_rg = TRange(src_rg.GetFrom()*3, src_rg.GetTo()*3 + 2);
    }
    else if (m_GraphRanges  &&  src_type == eSeq_unknown) {
        // Unknown sequence type, don't know how much of the graph
        // data to skip.
        ERR_POST_X(26, Warning <<
            "Unknown sequence type in the source location, "
            "mapped graph data may be incorrect.");
    }

    // Collect mappings which can be used to map the range.
    TSortedMappings mappings;
    TRangeIterator rg_it = m_Mappings->BeginMappingRanges(
        src_idh, src_rg.GetFrom(), src_rg.GetTo());
    for ( ; rg_it; ++rg_it) {
        mappings.push_back(rg_it->second);
    }
    // Sort the mappings depending on the original location strand.
    if ( IsReverse(src_strand) ) {
        sort(mappings.begin(), mappings.end(), CMappingRangeRef_LessRev());
    }
    else {
        sort(mappings.begin(), mappings.end(), CMappingRangeRef_Less());
    }

    // special adjustment (e.g. GU561555)
    // This should very *rarely* be needed
    if( ! m_Mappings.Empty() ) {
        // get first mapping
        TRangeIterator rg_it = m_Mappings->BeginMappingRanges(src_idh, 0, 1);
        if( rg_it && rg_it->second ) {
            const CMappingRange &mapping = *rg_it->second;
            // try to detect if we hit the case where we couldn't do a frame-shift
            if( ! mapping.m_Reverse && mapping.m_Frame > 1 && mapping.m_Dst_from == 0 &&
                mapping.m_Dst_len <= static_cast<TSeqPos>(mapping.m_Frame - 1)  )
            {
                const int shift = ( mappings[0]->m_Frame - 1 );
                if( src_rg.GetFrom() != 0 ) {
                    src_rg.SetFrom( src_rg.GetFrom() + shift );
                }
                src_rg.SetTo( src_rg.GetTo() + shift);
            }
        }
    }

    // The last mapped position (in biological order). Required to check
    // if some part of the source location did not match any mapping range
    // and was dropped.
    TSeqPos last_src_to = kInvalidSeqPos;
    // Save offset from the graph start to restore it later.
    TSeqPos graph_offset = m_GraphRanges ? m_GraphRanges->GetOffset() : 0;
    // Map through each mapping. If some part of the original range matches
    // several mappings, it will be mapped several times.
    for (size_t idx = 0; idx < mappings.size(); ++idx) {
        if ( x_MapNextRange(src_rg,
                            is_set_strand, src_strand,
                            orig_fuzz,
                            mappings, idx,
                            &last_src_to) ) {
            res = true;
        }
        // Mapping can adjust graph offset, but while mapping the same
        // source range we need to preserve it.
        if ( m_GraphRanges ) {
            m_GraphRanges->SetOffset(graph_offset);
        }
    }
    // If nothing could be mapped, set 'truncated' flag.
    if ( !res ) {
        x_SetLastTruncated();
    }
    // Now it's ok to adjust graph offset.
    if ( m_GraphRanges ) {
        if ( !src_rg.IsWhole() ) {
            m_GraphRanges->IncOffset(src_rg.GetLength());
        }
        else {
            ERR_POST_X(27, Warning <<
                "Unknown sequence length in the source whole location, "
                "mapped graph data may be incorrect.");
        }
    }
    return res;
}


void CSeq_loc_Mapper_Base::x_Map_PackedInt_Element(const CSeq_interval& si)
{
    TRangeFuzz fuzz(kEmptyFuzz, kEmptyFuzz);
    // Copy fuzz from the original interval.
    if ( si.IsSetFuzz_from() ) {
        fuzz.first.Reset(new CInt_fuzz);
        fuzz.first->Assign(si.GetFuzz_from());
    }
    if ( si.IsSetFuzz_to() ) {
        fuzz.second.Reset(new CInt_fuzz);
        fuzz.second->Assign(si.GetFuzz_to());
    }
    // Map the same way as a standalone seq-interval.
    bool res = x_MapInterval(si.GetId(),
        TRange(si.GetFrom(), si.GetTo()),
        si.IsSetStrand(),
        si.IsSetStrand() ? si.GetStrand() : eNa_strand_unknown,
        fuzz);
    if ( !res ) {
        // If the interval could not be mapped, we may need to keep
        // the original one.
        if ( m_KeepNonmapping ) {
            // Propagate collected mapped ranges to the destination seq-loc.
            x_PushRangesToDstMix();
            // Add a copy of the original interval.
            TRange rg(si.GetFrom(), si.GetTo());
            x_PushMappedRange(CSeq_id_Handle::GetHandle(si.GetId()),
                STRAND_TO_INDEX(si.IsSetStrand(), si.GetStrand()),
                rg, fuzz, false, 0);
        }
        else {
            // If we don't need to keep the non-mapping ranges, just mark
            // the result as partial.
            m_Partial = true;
        }
    }
}


void CSeq_loc_Mapper_Base::x_Map_PackedPnt_Element(const CPacked_seqpnt& pp,
                                                   TSeqPos p)
{
    TRangeFuzz fuzz(kEmptyFuzz, kEmptyFuzz);
    // Copy fuzz from the original point.
    if ( pp.IsSetFuzz() ) {
        fuzz.first.Reset(new CInt_fuzz);
        fuzz.first->Assign(pp.GetFuzz());
    }
    // Map the same way as a standalone seq-interval.
    bool res = x_MapInterval(
        pp.GetId(),
        TRange(p, p), pp.IsSetStrand(),
        pp.IsSetStrand() ?
        pp.GetStrand() : eNa_strand_unknown,
        fuzz);
    if ( !res ) {
        // If the point could not be mapped, we may need to keep
        // the original one.
        if ( m_KeepNonmapping ) {
            // Propagate collected mapped ranges to the destination seq-loc.
            x_PushRangesToDstMix();
            // Add a copy of the original point.
            TRange rg(p, p);
            x_PushMappedRange(
                CSeq_id_Handle::GetHandle(pp.GetId()),
                STRAND_TO_INDEX(pp.IsSetStrand(),
                                pp.GetStrand()),
                rg, fuzz, false, 0);
        }
        else {
            // If we don't need to keep the non-mapping ranges, just mark
            // the result as partial.
            m_Partial = true;
        }
    }
}


void CSeq_loc_Mapper_Base::x_MapSeq_loc(const CSeq_loc& src_loc)
{
    // Parse and map a seq-loc.
    switch ( src_loc.Which() ) {
    case CSeq_loc::e_Null:
        // Check if gaps are allowed in the result.
        if (m_GapFlag == eGapRemove) {
            return; // No - just ignore it.
        }
        // Yes - proceed to seq-loc duplication
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Feat:
    {
        // These types can not be mapped, just copy them to the
        // resulting seq-loc.
        // First, push any ranges already mapped to the result.
        x_PushRangesToDstMix();
        // Add a copy of the original location.
        CRef<CSeq_loc> loc(new CSeq_loc);
        loc->Assign(src_loc);
        x_PushLocToDstMix(loc);
        break;
    }
    case CSeq_loc::e_Empty:
    {
        // With empty seq-locs we can only change its seq-id.
        bool res = false;
        // Check if the id can be mapped at all.
        TRangeIterator mit = m_Mappings->BeginMappingRanges(
            x_GetPrimaryId(CSeq_id_Handle::GetHandle(src_loc.GetEmpty())),
            TRange::GetWhole().GetFrom(),
            TRange::GetWhole().GetTo());
        for ( ; mit; ++mit) {
            const CMappingRange& cvt = *mit->second;
            if ( cvt.GoodSrcId(src_loc.GetEmpty()) ) {
                // Found matching source id, map it to the destination.
                TRangeFuzz fuzz(kEmptyFuzz, kEmptyFuzz);
                x_PushMappedRange(
                    cvt.GetDstIdHandle(),
                    STRAND_TO_INDEX(false, eNa_strand_unknown),
                    TRange::GetEmpty(), fuzz, false, 0);
                res = true;
                break;
            }
        }
        if ( !res ) {
            // If we don't have any mappings for this seq-id we may
            // still need to keep the original.
            if ( m_KeepNonmapping ) {
                x_PushRangesToDstMix();
                CRef<CSeq_loc> loc(new CSeq_loc);
                loc->Assign(src_loc);
                x_PushLocToDstMix(loc);
            }
            else {
                m_Partial = true;
            }
        }
        break;
    }
    case CSeq_loc::e_Whole:
    {
        // Whole locations are mapped the same way as intervals, but we need
        // to know the bioseq length.
        const CSeq_id& src_id = src_loc.GetWhole();
        TSeqPos src_to = GetSequenceLength(src_id);
        TRange src_rg = TRange::GetWhole();
        // Sequence length returned above may be zero - treat it as unknown.
        if (src_to > 0  &&  src_to != kInvalidSeqPos) {
            src_rg.SetOpen(0, src_to);
        }
        // The length may still be unknown, but we'll try to map it anyway.
        // If there are no minus strands involved, it should be possible.
        bool res = x_MapInterval(src_id, src_rg,
            false, eNa_strand_unknown,
            TRangeFuzz(kEmptyFuzz, kEmptyFuzz));
        if ( !res ) {
            // If nothing could be mapped, we may still need to keep
            // the original.
            if ( m_KeepNonmapping ) {
                x_PushRangesToDstMix();
                CRef<CSeq_loc> loc(new CSeq_loc);
                loc->Assign(src_loc);
                x_PushLocToDstMix(loc);
            }
            else {
                m_Partial = true;
            }
        }
        break;
    }
    case CSeq_loc::e_Int:
    {
        // Map a single interval.
        const CSeq_interval& src_int = src_loc.GetInt();
        // Copy fuzz so that it's preserved if there are no truncations.
        TRangeFuzz fuzz(kEmptyFuzz, kEmptyFuzz);
        if ( src_int.IsSetFuzz_from() ) {
            fuzz.first.Reset(new CInt_fuzz);
            fuzz.first->Assign(src_int.GetFuzz_from());
        }
        if ( src_int.IsSetFuzz_to() ) {
            fuzz.second.Reset(new CInt_fuzz);
            fuzz.second->Assign(src_int.GetFuzz_to());
        }
        // Map the interval.
        bool res = x_MapInterval(src_int.GetId(),
            TRange(src_int.GetFrom(), src_int.GetTo()),
            src_int.IsSetStrand(),
            src_int.IsSetStrand() ? src_int.GetStrand() : eNa_strand_unknown,
            fuzz);
        if ( !res ) {
            // If nothing could be mapped, we may still need to keep
            // the original.
            if ( m_KeepNonmapping ) {
                x_PushRangesToDstMix();
                CRef<CSeq_loc> loc(new CSeq_loc);
                loc->Assign(src_loc);
                // This is the only difference from mapping a packed-int
                // element - we keep the whole original seq-loc rather than
                // a single interval.
                x_PushLocToDstMix(loc);
            }
            else {
                m_Partial = true;
            }
        }
        break;
    }
    case CSeq_loc::e_Pnt:
    {
        // Point is mapped as an interval of length 1.
        const CSeq_point& pnt = src_loc.GetPnt();
        TRangeFuzz fuzz(kEmptyFuzz, kEmptyFuzz);
        if ( pnt.IsSetFuzz() ) {
            // With C-style, we sometimes set the fuzz to the "to-fuzz" depending
            // on what the fuzz actually is.
            if( (m_FuzzOption & fFuzzOption_CStyle) != 0 && 
                (pnt.GetFuzz().IsLim() && 
                    pnt.GetFuzz().GetLim() == CInt_fuzz::eLim_gt) ) 
            {
                fuzz.second.Reset(new CInt_fuzz);
                fuzz.second->Assign(pnt.GetFuzz());
            } else {
                fuzz.first.Reset(new CInt_fuzz);
                fuzz.first->Assign(pnt.GetFuzz());
            }
        }
        bool res = x_MapInterval(pnt.GetId(),
            TRange(pnt.GetPoint(), pnt.GetPoint()),
            pnt.IsSetStrand(),
            pnt.IsSetStrand() ? pnt.GetStrand() : eNa_strand_unknown,
            fuzz);
        if ( !res ) {
            // If nothing could be mapped, we may still need to keep
            // the original.
            if ( m_KeepNonmapping ) {
                x_PushRangesToDstMix();
                CRef<CSeq_loc> loc(new CSeq_loc);
                loc->Assign(src_loc);
                x_PushLocToDstMix(loc);
            }
            else {
                m_Partial = true;
            }
        }
        break;
    }
    case CSeq_loc::e_Packed_int:
    {
        // Packed intervals are mapped one-by-one with
        const CPacked_seqint::Tdata& src_ints = src_loc.GetPacked_int().Get();
        ITERATE ( CPacked_seqint::Tdata, i, src_ints ) {
            x_Map_PackedInt_Element(**i);
        }
        break;
    }
    case CSeq_loc::e_Packed_pnt:
    {
        // Mapping of packed points is rather straightforward.
        const CPacked_seqpnt& src_pack_pnts = src_loc.GetPacked_pnt();
        const CPacked_seqpnt::TPoints& src_pnts = src_pack_pnts.GetPoints();
        ITERATE ( CPacked_seqpnt::TPoints, i, src_pnts ) {
            x_Map_PackedPnt_Element(src_pack_pnts, *i);
        }
        break;
    }
    case CSeq_loc::e_Mix:
    {
        // First, move any ranges already mapped to the resulting seq-loc.
        x_PushRangesToDstMix();
        // Save the resulting seq-loc for later use and reset it.
        CRef<CSeq_loc> prev = m_Dst_loc;
        m_Dst_loc.Reset();
        // Map each child seq-loc. The results are collected in m_Dst_loc
        // as a new mix.
        const CSeq_loc_mix::Tdata& src_mix = src_loc.GetMix().Get();
        ITERATE ( CSeq_loc_mix::Tdata, i, src_mix ) {
            x_MapSeq_loc(**i);
        }
        // Update the mapped location if necessary.
        x_PushRangesToDstMix();
        // Restore the previous (e.g. parent mix) mapped location if any.
        CRef<CSeq_loc> mix = m_Dst_loc;
        m_Dst_loc = prev;
        // Optimize the mix just mapped and push it to the parent one.
        x_OptimizeSeq_loc(mix);
        x_PushLocToDstMix(mix);
        break;
    }
    case CSeq_loc::e_Equiv:
    {
        // Equiv is mapped basically the same way as a mix:
        // map each sub-location, optimize the result and push it to the
        // destination equiv.
        x_PushRangesToDstMix();
        CRef<CSeq_loc> prev = m_Dst_loc;
        m_Dst_loc.Reset();
        const CSeq_loc_equiv::Tdata& src_equiv = src_loc.GetEquiv().Get();
        CRef<CSeq_loc> equiv(new CSeq_loc);
        equiv->SetEquiv();
        ITERATE ( CSeq_loc_equiv::Tdata, i, src_equiv ) {
            x_MapSeq_loc(**i);
            x_PushRangesToDstMix();
            x_OptimizeSeq_loc(m_Dst_loc);
            equiv->SetEquiv().Set().push_back(m_Dst_loc);
            m_Dst_loc.Reset();
        }
        m_Dst_loc = prev;
        x_PushLocToDstMix(equiv);
        break;
    }
    case CSeq_loc::e_Bond:
    {
        // Bond is mapped like a mix having two sub-locations (A and B).
        x_PushRangesToDstMix();
        CRef<CSeq_loc> prev = m_Dst_loc;
        m_Dst_loc.Reset();
        const CSeq_bond& src_bond = src_loc.GetBond();
        CRef<CSeq_loc> dst_loc(new CSeq_loc);
        CRef<CSeq_loc> pntA;
        CRef<CSeq_loc> pntB;
        TRangeFuzz fuzzA(kEmptyFuzz, kEmptyFuzz);
        if ( src_bond.GetA().IsSetFuzz() ) {
            fuzzA.first.Reset(new CInt_fuzz);
            fuzzA.first->Assign(src_bond.GetA().GetFuzz());
        }
        bool resA = x_MapInterval(src_bond.GetA().GetId(),
            TRange(src_bond.GetA().GetPoint(), src_bond.GetA().GetPoint()),
            src_bond.GetA().IsSetStrand(),
            src_bond.GetA().IsSetStrand() ?
            src_bond.GetA().GetStrand() : eNa_strand_unknown,
            fuzzA);
        // If A or B could not be mapped, always preserve the original one
        // regardless of the KeepNonmapping flag - we can not just
        // drop a part of a bond. See more below.
        if ( resA ) {
            pntA = x_GetMappedSeq_loc();
            _ASSERT(pntA);
        }
        else {
            pntA.Reset(new CSeq_loc);
            pntA->SetPnt().Assign(src_bond.GetA());
        }
        // Reset truncation flag - we are starting new location.
        m_LastTruncated = false;
        bool resB = false;
        if ( src_bond.IsSetB() ) {
            TRangeFuzz fuzzB(kEmptyFuzz, kEmptyFuzz);
            if ( src_bond.GetB().IsSetFuzz() ) {
                fuzzB.first.Reset(new CInt_fuzz);
                fuzzB.first->Assign(src_bond.GetB().GetFuzz());
            }
            resB = x_MapInterval(src_bond.GetB().GetId(),
                TRange(src_bond.GetB().GetPoint(), src_bond.GetB().GetPoint()),
                src_bond.GetB().IsSetStrand(),
                src_bond.GetB().IsSetStrand() ?
                src_bond.GetB().GetStrand() : eNa_strand_unknown,
                fuzzB);
        }
        if ( resB ) {
            pntB = x_GetMappedSeq_loc();
            _ASSERT(pntB);
        }
        else {
            pntB.Reset(new CSeq_loc);
            pntB->SetPnt().Assign(src_bond.GetB());
        }
        m_Dst_loc = prev;
        // Now we check the non-mapping flag. Only if both A and B
        // failed to map and the flag is not set, we can discard the bond.
        if ( resA  ||  resB  ||  m_KeepNonmapping ) {
            if (pntA->IsPnt()  &&  pntB->IsPnt()) {
                // Mapped locations are points - pack into bond
                CSeq_bond& dst_bond = dst_loc->SetBond();
                dst_bond.SetA(pntA->SetPnt());
                if ( src_bond.IsSetB() ) {
                    dst_bond.SetB(pntB->SetPnt());
                }
            }
            else {
                // The original points were mapped to something different
                // (e.g. there were multiple mappings for each point).
                // Convert the whole bond to mix, add gaps between A and B.
                CSeq_loc_mix& dst_mix = dst_loc->SetMix();
                if ( pntA ) {
                    dst_mix.Set().push_back(pntA);
                }
                if ( pntB ) {
                    // Add null only if B is set.
                    CRef<CSeq_loc> null_loc(new CSeq_loc);
                    null_loc->SetNull();
                    dst_mix.Set().push_back(null_loc);
                    dst_mix.Set().push_back(pntB);
                }
            }
            x_PushLocToDstMix(dst_loc);
        }
        m_Partial = m_Partial  ||  (!resA)  ||  (!resB);
        break;
    }
    default:
        NCBI_THROW(CAnnotMapperException, eBadLocation,
                   "Unsupported location type");
    }
}

CSeq_align_Mapper_Base*
CSeq_loc_Mapper_Base::InitAlignMapper(const CSeq_align& src_align)
{
    // Here we create an alignment mapper to map aligns.
    // CSeq_loc_Mapper overrides this to return CSeq_align_Mapper.
    return new CSeq_align_Mapper_Base(src_align, *this);
}


CRef<CSeq_loc> CSeq_loc_Mapper_Base::Map(const CSeq_loc& src_loc)
{
    // Reset the mapper before mapping each location
    m_Dst_loc.Reset();
    m_Partial = false;
    m_LastTruncated = false;
    x_MapSeq_loc(src_loc);
    // Push any remaining mapped ranges to the mapped location.
    x_PushRangesToDstMix();
    // C-style generates less fuzz, so we would then have to remove some
    if( (m_FuzzOption & fFuzzOption_CStyle) != 0 ) {
        x_StripExtraneousFuzz(m_Dst_loc);
    }
    // Optimize mapped location.
    x_OptimizeSeq_loc(m_Dst_loc);
    // If source locations should be included, optimize them too and
    // convert the result to equiv.
    if ( m_SrcLocs ) {
        x_OptimizeSeq_loc(m_SrcLocs);
        CRef<CSeq_loc> ret(new CSeq_loc);
        ret->SetEquiv().Set().push_back(m_Dst_loc);
        ret->SetEquiv().Set().push_back(m_SrcLocs);
        return ret;
    }
    return m_Dst_loc;
}


CRef<CSeq_align>
CSeq_loc_Mapper_Base::x_MapSeq_align(const CSeq_align& src_align,
                                     size_t*           row)
{
    // Mapping of alignments if performed by seq-align mapper.
    m_Dst_loc.Reset();
    m_Partial = false;
    m_LastTruncated = false;
    CRef<CSeq_align_Mapper_Base> aln_mapper(InitAlignMapper(src_align));
    if ( row ) {
        aln_mapper->Convert(*row);
    }
    else {
        aln_mapper->Convert();
    }
    return aln_mapper->GetDstAlign();
}


/////////////////////////////////////////////////////////////////////
//
//   Produce result of the mapping
//


CRef<CSeq_loc> CSeq_loc_Mapper_Base::
x_RangeToSeq_loc(const CSeq_id_Handle& idh,
                 TSeqPos               from,
                 TSeqPos               to,
                 size_t                strand_idx,
                 TRangeFuzz            rg_fuzz)
{
    ESeqType seq_type = GetSeqTypeById(idh);
    if (seq_type == eSeq_prot) {
        // Convert coordinates. For seq-locs discard frame information.
        from = from/3;
        to = to/3;
    }

    CRef<CSeq_loc> loc(new CSeq_loc);
    // If any fuzz is set, create interval, not point.
    // Points with fuzz can create problems later since they don't
    // specify fuzz direction. See GP-2895.
    if (from == to  &&  (!rg_fuzz.first  &&  !rg_fuzz.second) &&
        (m_FuzzOption & fFuzzOption_CStyle) == 0 )
    {
        // point
        loc->SetPnt().SetId().Assign(*idh.GetSeqId());
        loc->SetPnt().SetPoint(from);
        if (strand_idx > 0) {
            loc->SetPnt().SetStrand(INDEX_TO_STRAND(strand_idx));
        }
        if ( rg_fuzz.first ) {
            loc->SetPnt().SetFuzz(*rg_fuzz.first);
        }
        else if ( rg_fuzz.second ) {
            loc->SetPnt().SetFuzz(*rg_fuzz.second);
        }
    }
    // Note: at this moment for whole locations 'to' is equal to GetWholeTo()
    // not GetWholeToOpen().
    else if (from == 0  &&  to == TRange::GetWholeTo()) {
        loc->SetWhole().Assign(*idh.GetSeqId());
        // Ignore strand for whole locations
    }
    else {
        // interval
        loc->SetInt().SetId().Assign(*idh.GetSeqId());
        loc->SetInt().SetFrom(from);
        loc->SetInt().SetTo(to);
        if (strand_idx > 0) {
            loc->SetInt().SetStrand(INDEX_TO_STRAND(strand_idx));
        }
        if ( rg_fuzz.first ) {
            loc->SetInt().SetFuzz_from(*rg_fuzz.first);
        }
        if ( rg_fuzz.second ) {
            loc->SetInt().SetFuzz_to(*rg_fuzz.second);
        }
    }
    return loc;
}


CSeq_loc_Mapper_Base::TMappedRanges&
CSeq_loc_Mapper_Base::x_GetMappedRanges(const CSeq_id_Handle& id,
                                        size_t                strand_idx) const
{
    // Get mapped ranges for the given id and strand.
    // Make sure the vector contains entry for the strand index.
    TRangesByStrand& str_vec = m_MappedLocs[id];
    if (str_vec.size() <= strand_idx) {
        str_vec.resize(strand_idx + 1);
    }
    return str_vec[strand_idx];
}


// Add new mapped range.
// The range is added as the first or the last element depending on its strand.
// 'push_reverse' indicates if this rule must be reversed. This flag is set
// when the mapping itself reverses the strand.
void CSeq_loc_Mapper_Base::x_PushMappedRange(const CSeq_id_Handle& id,
                                             size_t                strand_idx,
                                             const TRange&         range,
                                             const TRangeFuzz&     fuzz,
                                             bool                  push_reverse,
                                             int                   group)
{
    // It is impossible to collect source locations and do merging
    // at the same time.
    if (m_IncludeSrcLocs  &&  m_MergeFlag != eMergeNone) {
        NCBI_THROW(CAnnotMapperException, eOtherError,
                   "Merging ranges is incompatible with "
                   "including source locations.");
    }
    bool reverse = (strand_idx > 0) &&
        IsReverse(INDEX_TO_STRAND(strand_idx));
    switch ( m_MergeFlag ) {
    case eMergeContained:
    case eMergeAll:
        {
            // Merging will be done later, while constructing the mapped
            // seq-loc. Now just add new range in the right order.
            if ( push_reverse ) {
                x_GetMappedRanges(id, strand_idx)
                    .push_front(SMappedRange(range, fuzz, group));
            }
            else {
                x_GetMappedRanges(id, strand_idx)
                    .push_back(SMappedRange(range, fuzz, group));
            }
            break;
        }
    case eMergeNone:
        {
            // No merging. Propagate any collected ranges to the
            // mapped location to keep grouping, add the new one.
            x_PushRangesToDstMix();
            if ( push_reverse ) {
                x_GetMappedRanges(id, strand_idx)
                    .push_front(SMappedRange(range, fuzz, group));
            }
            else {
                x_GetMappedRanges(id, strand_idx)
                    .push_back(SMappedRange(range, fuzz, group));
            }
            break;
        }
    case eMergeAbutting:
    case eMergeBySeg:
    default:
        {
            // Some special processing is required.
            TRangesById::iterator it = m_MappedLocs.begin();
            // Start new sub-location for:
            // - New ID (can not merge ranges on different sequences)
            bool no_merge = (it == m_MappedLocs.end())  ||  (it->first != id);
            // - New strand (can not merge ranges on different strands)
            no_merge = no_merge  ||
                (it->second.size() <= strand_idx)  ||  it->second.empty();
            // - Ranges are not abutting or belong to different groups
            if ( !no_merge ) {
                // Compare the new range to the previous one, which can be
                // the first or the last depending on the strand.
                if ( reverse ) {
                    const SMappedRange& mrg = it->second[strand_idx].front();
                    // Check coordinates or group number.
                    if (m_MergeFlag == eMergeAbutting) {
                        no_merge = no_merge ||
                            (mrg.range.GetFrom() != range.GetToOpen());
                    }
                    if (m_MergeFlag == eMergeBySeg) {
                        no_merge = no_merge  ||  (mrg.group != group);
                    }
                }
                else {
                    const SMappedRange& mrg = it->second[strand_idx].back();
                    // Check coordinates or group number.
                    if (m_MergeFlag == eMergeAbutting) {
                        no_merge = no_merge  ||
                            (mrg.range.GetToOpen() != range.GetFrom());
                    }
                    if (m_MergeFlag == eMergeBySeg) {
                        no_merge = no_merge  ||  (mrg.group != group);
                    }
                }
            }
            if ( no_merge ) {
                // Can not merge the new range with the previous one.
                x_PushRangesToDstMix();
                if ( push_reverse ) {
                    x_GetMappedRanges(id, strand_idx)
                        .push_front(SMappedRange(range, fuzz, group));
                }
                else {
                    x_GetMappedRanges(id, strand_idx)
                        .push_back(SMappedRange(range, fuzz, group));
                }
            }
            else {
                // The ranges can be merged. Take the strand into account.
                if ( reverse ) {
                    SMappedRange& last_rg = it->second[strand_idx].front();
                    last_rg.range.SetFrom(range.GetFrom());
                    last_rg.fuzz.first = fuzz.first;
                }
                else {
                    SMappedRange& last_rg = it->second[strand_idx].back();
                    last_rg.range.SetTo(range.GetTo());
                    last_rg.fuzz.second = fuzz.second;
                }
            }
        }
    }
}


// Store the range from the original location which could be mapped.
// See also x_PushMappedRange.
void CSeq_loc_Mapper_Base::x_PushSourceRange(const CSeq_id_Handle& idh,
                                             size_t                src_strand,
                                             size_t                dst_strand,
                                             const TRange&         range,
                                             bool                  push_reverse)
{
    if ( !m_IncludeSrcLocs ) return; // No need to store source ranges.
    if ( !m_SrcLocs ) {
        m_SrcLocs.Reset(new CSeq_loc);
    }
    CRef<CSeq_loc> loc(new CSeq_loc);
    CRef<CSeq_id> id(new CSeq_id);
    id->Assign(*idh.GetSeqId());
    if ( range.Empty() ) {
        loc->SetEmpty(*id);
    }
    else if ( range.IsWhole() ) {
        loc->SetWhole(*id);
    }
    else {
        // The range uses genomic coords, recalculate if necessary.
        ESeqType seq_type = GetSeqTypeById(idh);
        int seq_width = (seq_type == eSeq_prot) ? 3 : 1;
        loc->SetInt().SetId(*id);
        loc->SetInt().SetFrom(range.GetFrom()/seq_width);
        loc->SetInt().SetTo(range.GetTo()/seq_width);
        if (src_strand > 0) {
            loc->SetStrand(INDEX_TO_STRAND(src_strand));
        }
    }
    // Store the location.
    if ( !SameOrientation(
        src_strand ? INDEX_TO_STRAND(src_strand) : eNa_strand_unknown,
        dst_strand ? INDEX_TO_STRAND(dst_strand) : eNa_strand_unknown) ) {
        push_reverse = !push_reverse;
    }
    if ( push_reverse ) {
        m_SrcLocs->SetMix().Set().push_front(loc);
    }
    else {
        m_SrcLocs->SetMix().Set().push_back(loc);
    }
}


void CSeq_loc_Mapper_Base::x_PushRangesToDstMix(void)
{
    // Are there any locations ready?
    if (m_MappedLocs.size() == 0) {
        return;
    }
    // Push everything already mapped to the destination mix.
    // m_MappedLocs are reset and ready to accept the next part.
    CRef<CSeq_loc> loc = x_GetMappedSeq_loc();
    if ( !m_Dst_loc ) {
        // If this is the first mapped location, just use it without
        // wrapping in a mix.
        m_Dst_loc = loc;
        return;
    }
    if ( !loc->IsNull() ) {
        // If the location is not null, add it to the existing mix.
        x_PushLocToDstMix(loc);
    }
}


void CSeq_loc_Mapper_Base::x_PushLocToDstMix(CRef<CSeq_loc> loc)
{
    _ASSERT(loc);
    // If the mix does not exist yet, create it.
    if ( !m_Dst_loc  ||  !m_Dst_loc->IsMix() ) {
        CRef<CSeq_loc> tmp = m_Dst_loc;
        m_Dst_loc.Reset(new CSeq_loc);
        m_Dst_loc->SetMix();
        if ( tmp ) {
            m_Dst_loc->SetMix().Set().push_back(tmp);
        }
    }
    CSeq_loc_mix::Tdata& mix = m_Dst_loc->SetMix().Set();
    if ( loc->IsNull() ) {
        if ( m_GapFlag == eGapRemove ) {
            return; // No need to store gaps
        }
        if ( mix.size() > 0  &&  (*mix.rbegin())->IsNull() ) {
            // do not create duplicate NULLs
            return;
        }
    }
    mix.push_back(loc);
}


bool CSeq_loc_Mapper_Base::x_ReverseRangeOrder(int str) const
{
    if (m_MergeFlag == eMergeContained  || m_MergeFlag == eMergeAll) {
        // Sorting discards the original order, no need to check
        // mappings, just use the mapped strand.
        return str != 0  &&  IsReverse(INDEX_TO_STRAND(str));
    }
    // For other merging modes the strand is not important (it's checked
    // somewhere else), we just need to know if the order of ranges
    // is reversed by mapping or not.
    return m_Mappings->GetReverseSrc() != m_Mappings->GetReverseDst();
}


CRef<CSeq_loc> CSeq_loc_Mapper_Base::x_GetMappedSeq_loc(void)
{
    // Create a new mix to store all mapped ranges in it.
    CRef<CSeq_loc> dst_loc(new CSeq_loc);
    CSeq_loc_mix::Tdata& dst_mix = dst_loc->SetMix().Set();
    // Iterate all mapped seq-ids.
    NON_CONST_ITERATE(TRangesById, id_it, m_MappedLocs) {
        // Uninitialized id means gap (this should not happen in fact).
        if ( !id_it->first ) {
            if (m_GapFlag == eGapPreserve) {
                CRef<CSeq_loc> null_loc(new CSeq_loc);
                null_loc->SetNull();
                dst_mix.push_back(null_loc);
            }
            continue;
        }
        // Iterate all strands for the current id.
        for (int str = 0; str < (int)id_it->second.size(); ++str) {
            if (id_it->second[str].size() == 0) {
                continue;
            }
            TSeqPos from = kInvalidSeqPos;
            TSeqPos to = kInvalidSeqPos;
            TRangeFuzz fuzz(kEmptyFuzz, kEmptyFuzz);
            // Some merge flags require the ranges to be sorted.
            if (m_MergeFlag == eMergeContained  || m_MergeFlag == eMergeAll) {
                id_it->second[str].sort();
            }
            // Iterate mapped ranges.
            NON_CONST_ITERATE(TMappedRanges, rg_it, id_it->second[str]) {
                if ( rg_it->range.Empty() ) {
                    // Empty seq-loc
                    CRef<CSeq_loc> loc(new CSeq_loc);
                    loc->SetEmpty().Assign(*id_it->first.GetSeqId());
                    if ( x_ReverseRangeOrder(0) ) {
                        dst_mix.push_front(loc);
                    }
                    else {
                        dst_mix.push_back(loc);
                    }
                    continue;
                }
                // Is this the first mapped range?
                if (to == kInvalidSeqPos) {
                    // Initialize from, to and fuzz.
                    from = rg_it->range.GetFrom();
                    to = rg_it->range.GetTo();
                    fuzz = rg_it->fuzz;
                    continue;
                }
                // Merge abutting ranges. The ranges are sorted by 'from',
                // so we need to check only one end.
                if (m_MergeFlag == eMergeAbutting) {
                    if (rg_it->range.GetFrom() == to + 1) {
                        to = rg_it->range.GetTo();
                        fuzz.second = rg_it->fuzz.second;
                        continue;
                    }
                }
                // Merge contained ranges
                if (m_MergeFlag == eMergeContained) {
                    // Ignore interval completely covered by another one.
                    // Check only 'to', since the ranges are sorted by 'from'.
                    if (rg_it->range.GetTo() <= to) {
                        continue;
                    }
                    // If the old range is contaied in the new one, adjust
                    // its 'to'.
                    if (rg_it->range.GetFrom() == from) {
                        to = rg_it->range.GetTo();
                        fuzz.second = rg_it->fuzz.second;
                        continue;
                    }
                }
                // Merge all overlapping ranges.
                if (m_MergeFlag == eMergeAll) {
                    if (rg_it->range.GetFrom() <= to + 1) {
                        if (rg_it->range.GetTo() > to) {
                            to = rg_it->range.GetTo();
                            fuzz.second = rg_it->fuzz.second;
                        }
                        continue;
                    }
                }
                // No merging happened - store the previous interval
                // or point.
                if ( x_ReverseRangeOrder(str) ) {
                    dst_mix.push_front(x_RangeToSeq_loc(id_it->first, from, to,
                        str, fuzz));
                }
                else {
                    dst_mix.push_back(x_RangeToSeq_loc(id_it->first, from, to,
                        str, fuzz));
                }
                // Initialize the new range, but do not store it yet - it
                // may be merged with the next one.
                from = rg_it->range.GetFrom();
                to = rg_it->range.GetTo();
                fuzz = rg_it->fuzz;
            }
            // If there were only empty ranges, do not try to add them as points.
            if (from == kInvalidSeqPos  &&  to == kInvalidSeqPos) {
                continue;
            }
            // Last interval or point not yet stored.
            if ( x_ReverseRangeOrder(str) ) {
                dst_mix.push_front(x_RangeToSeq_loc(id_it->first, from, to,
                    str, fuzz));
            }
            else {
                dst_mix.push_back(x_RangeToSeq_loc(id_it->first, from, to,
                    str, fuzz));
            }
        }
    }
    m_MappedLocs.clear();
    x_OptimizeSeq_loc(dst_loc);
    return dst_loc;
}


// Copy a range from the original graph data to the mapped one.
template<class TData> void CopyGraphData(const TData& src,
                                         TData&       dst,
                                         TSeqPos      from,
                                         TSeqPos      to)
{
    _ASSERT(from < src.size()  &&  to <= src.size());
    dst.insert(dst.end(), src.begin() + from, src.begin() + to);
}


CRef<CSeq_graph> CSeq_loc_Mapper_Base::Map(const CSeq_graph& src_graph)
{
    CRef<CSeq_graph> ret;
    // Start collecting used ranges to adjust graph data.
    m_GraphRanges.Reset(new CGraphRanges);
    CRef<CSeq_loc> mapped_loc = Map(src_graph.GetLoc());
    if ( !mapped_loc ) {
        // Nothing was mapped, return NULL.
        return ret;
    }
    ret.Reset(new CSeq_graph);
    ret->Assign(src_graph);
    ret->SetLoc(*mapped_loc);

    // Check mapped sequence type, adjust coordinates.
    ESeqType src_type = eSeq_unknown;
    bool src_type_set = false;
    // Iterate the original location, look for the sequence type.
    for (CSeq_loc_CI it = src_graph.GetLoc(); it; ++it) {
        ESeqType it_type = GetSeqTypeById(it.GetSeq_id_Handle());
        if (it_type == eSeq_unknown) {
            continue;
        }
        if ( !src_type_set ) {
            src_type = it_type;
            src_type_set = true;
        }
        else if (src_type != it_type) {
            NCBI_THROW(CAnnotMapperException, eBadLocation,
                "Source graph location contains different sequence "
                "types -- can not map graph data.");
        }
    }
    ESeqType dst_type = eSeq_unknown;
    bool dst_type_set = false;
    // Iterate the mapped location, look for the sequence type.
    for (CSeq_loc_CI it = *mapped_loc; it; ++it) {
        ESeqType it_type = GetSeqTypeById(it.GetSeq_id_Handle());
        if (it_type == eSeq_unknown) {
            continue;
        }
        if ( !dst_type_set ) {
            dst_type = it_type;
            dst_type_set = true;
        }
        else if (dst_type != it_type) {
            NCBI_THROW(CAnnotMapperException, eBadLocation,
                "Mapped graph location contains different sequence "
                "types -- can not map graph data.");
        }
    }

    CSeq_graph::TGraph& dst_data = ret->SetGraph();
    dst_data.Reset();
    const CSeq_graph::TGraph& src_data = src_graph.GetGraph();

    // Recalculate compression factor.
    TSeqPos comp = (src_graph.IsSetComp()  &&  src_graph.GetComp()) ?
        src_graph.GetComp() : 1;
    // In some cases the original data indexing must be divided by 3
    // to get mapped data indexes.
    TSeqPos comp_div = comp;
    // By now, only one sequence type can be present.
    // If the original and mapped sequence types are different
    // and one of them is prot, adjust compression.
    if (src_type != dst_type  &&
        (src_type == eSeq_prot  ||  dst_type == eSeq_prot)) {
        // Source is prot, need to multiply comp by 3
        if (src_type == eSeq_prot) {
            comp *= 3;
            comp_div = comp;
        }
        // Mapped is prot, need to divide comp by 3 if possible
        else if (comp % 3 == 0) {
            comp /= 3;
        }
        else {
            // Can not divide by 3, impossible to adjust data.
            NCBI_THROW(CAnnotMapperException, eOtherError,
                       "Can not map seq-graph data between "
                       "different sequence types.");
        }
    }
    ret->SetComp(comp);
    TSeqPos numval = 0;

    typedef CGraphRanges::TGraphRanges TGraphRanges;
    const TGraphRanges& ranges = m_GraphRanges->GetRanges();
    // Copy only the used ranges from the original data to the mapped one.
    switch ( src_data.Which() ) {
    case CSeq_graph::TGraph::e_Byte:
        dst_data.SetByte().SetMin(src_data.GetByte().GetMin());
        dst_data.SetByte().SetMax(src_data.GetByte().GetMax());
        dst_data.SetByte().SetAxis(src_data.GetByte().GetAxis());
        dst_data.SetByte().SetValues();
        // Copy each used range.
        ITERATE(TGraphRanges, it, ranges) {
            TSeqPos from = it->GetFrom()/comp_div;
            TSeqPos to = it->GetTo()/comp_div + 1;
            CopyGraphData(src_data.GetByte().GetValues(),
                dst_data.SetByte().SetValues(),
                from, to);
            numval += to - from;
        }
        break;
    case CSeq_graph::TGraph::e_Int:
        dst_data.SetInt().SetMin(src_data.GetInt().GetMin());
        dst_data.SetInt().SetMax(src_data.GetInt().GetMax());
        dst_data.SetInt().SetAxis(src_data.GetInt().GetAxis());
        dst_data.SetInt().SetValues();
        ITERATE(TGraphRanges, it, ranges) {
            TSeqPos from = it->GetFrom()/comp_div;
            TSeqPos to = it->GetTo()/comp_div + 1;
            CopyGraphData(src_data.GetInt().GetValues(),
                dst_data.SetInt().SetValues(),
                from, to);
            numval += to - from;
        }
        break;
    case CSeq_graph::TGraph::e_Real:
        dst_data.SetReal().SetMin(src_data.GetReal().GetMin());
        dst_data.SetReal().SetMax(src_data.GetReal().GetMax());
        dst_data.SetReal().SetAxis(src_data.GetReal().GetAxis());
        dst_data.SetReal().SetValues();
        ITERATE(TGraphRanges, it, ranges) {
            TSeqPos from = it->GetFrom()/comp_div;
            TSeqPos to = it->GetTo()/comp_div + 1;
            CopyGraphData(src_data.GetReal().GetValues(),
                dst_data.SetReal().SetValues(),
                from, to);
            numval += to - from;
        }
        break;
    default:
        break;
    }
    ret->SetNumval(numval);

    m_GraphRanges.Reset();
    return ret;
}


void CSeq_loc_Mapper_Base::Map(CSeq_annot& annot)
{
    switch (annot.GetData().Which()) {
    case CSeq_annot::C_Data::e_Ftable:
        {
            CSeq_annot::C_Data::TFtable& ftable = annot.SetData().SetFtable();
            NON_CONST_ITERATE(CSeq_annot::C_Data::TFtable, it, ftable) {
                CSeq_feat& feat = **it;
                CRef<CSeq_loc> loc;
                loc = Map(feat.GetLocation());
                if ( loc ) {
                    feat.SetLocation(*loc);
                }
                if ( feat.IsSetProduct() ) {
                    loc = Map(feat.GetProduct());
                    if ( loc ) {
                        feat.SetProduct(*loc);
                    }
                }
            }
            break;
        }
    case CSeq_annot::C_Data::e_Align:
        {
            CSeq_annot::C_Data::TAlign& aligns = annot.SetData().SetAlign();
            NON_CONST_ITERATE(CSeq_annot::C_Data::TAlign, it, aligns) {
                CRef<CSeq_align> align = Map(**it);
                if ( align ) {
                    *it = align;
                }
            }
            break;
        }
    case CSeq_annot::C_Data::e_Graph:
        {
            CSeq_annot::C_Data::TGraph& graphs = annot.SetData().SetGraph();
            NON_CONST_ITERATE(CSeq_annot::C_Data::TGraph, it, graphs) {
                CRef<CSeq_graph> graph = Map(**it);
                if ( graph ) {
                    *it = graph;
                }
            }
            break;
        }
    default:
        {
            ERR_POST_X(30, Warning << "Unsupported CSeq_annot type: " <<
                annot.GetData().Which());
            return;
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
