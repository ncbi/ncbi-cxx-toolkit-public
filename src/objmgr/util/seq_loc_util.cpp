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
* Author:  Clifford Clausen, Aaron Ucko, Aleksey Grichenko
*
* File Description:
*   Seq-loc utilities requiring CScope
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/scope.hpp>
#include <objects/seq/seq_id_mapper.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(sequence)


// Types used in operations with seq-locs
typedef CSeq_loc::TRange                    TRange;
typedef CRangeCollection<TSeqPos>           TRangeMap;
typedef map<CSeq_id_Handle, TRangeMap>      TIdToRangeMap;

void x_AddRangeToLoc(CSeq_loc& src_loc,
                     CSeq_id& id,
                     TRange range,
                     ENa_strand strand)
{
    if ( range.IsWhole() ) {
        // Strand is ignored!!!
        CRef<CSeq_loc> loc(new CSeq_loc);
        loc->SetWhole(id);
        src_loc.Add(*loc);
        return;
    }
    switch ( src_loc.Which() ) {
    case CSeq_loc::e_not_set:
        {
            src_loc.SetInt(*(new
                CSeq_interval(id, range.GetFrom(), range.GetTo(), strand)));
            return;
        }
    default:
        {
            // Use mix for any other type
            CRef<CSeq_loc> loc(new CSeq_loc);
            loc->SetInt(*(new
                CSeq_interval(id, range.GetFrom(), range.GetTo(), strand)));
            src_loc.ChangeToMix();
            src_loc.SetMix().AddSeqLoc(*loc);
            return;
        }
    }
}


void x_AddRangeMapToLoc(CSeq_loc& src_loc,
                        CSeq_id& id,
                        const TRangeMap& rmap,
                        ENa_strand strand)
{
    if (rmap.GetFrom() == TRange::GetWholeFrom()  &&
        rmap.GetTo() == TRange::GetWholeTo()) {
        // There must be just one whole range in the map
        _ASSERT(rmap.size() == 1);
        // Strand is ignored!!!
        CRef<CSeq_loc> loc(new CSeq_loc);
        loc->SetWhole(id);
        src_loc.Add(*loc);
        return;
    }
    if (rmap.size() == 1) {
        x_AddRangeToLoc(src_loc, id, *rmap.begin(), strand);
    }
    else {
        src_loc.ChangeToMix();
        ITERATE(TRangeMap, rg, rmap) {
            CRef<CSeq_loc> loc(new CSeq_loc);
            loc->SetInt(*(new
                CSeq_interval(id, rg->GetFrom(), rg->GetTo(), strand)));
            src_loc.SetMix().AddSeqLoc(*loc);
        }
    }
}


void x_AddNonOverlappingRangeToLoc(CSeq_loc& src_loc,
                                   CSeq_id& id, TRange rg,
                                   ENa_strand strand,
                                   const TRangeMap& rmap)
{
    TRangeMap diff(rg);
    diff -= rmap;
    x_AddRangeMapToLoc(src_loc, id, diff, strand);
}


// Scans loc and adds all ranges to id_map_* depending on strand.
// If merge_flag is eMerge, also adds resulting ranges to the destination
// seq-loc; if the flag is eMergeAndSort collects everything to id_map_*.
void x_ProcessSeqLoc(const CSeq_loc& loc,
                     EMergeFlag           merge_flag,
                     CSynonymMapper_Base& syn_mapper,
                     CLengthGetter_Base*  len_getter,
                     TIdToRangeMap&       id_map_plus,
                     TIdToRangeMap&       id_map_minus,
                     CSeq_loc&            res)
{
    for (CSeq_loc_CI it(loc); it; ++it) {
        CSeq_id_Handle idh = syn_mapper.GetBestSynonym(it.GetSeq_id());
        TRange rg = it.GetRange();
        if ( it.IsWhole()  &&  len_getter ) {
            rg.SetOpen(0, len_getter->GetLength(it.GetSeq_id()));
        }
        if (merge_flag == eMerge) {
            // Fill the destination seq-loc
            CRef<CSeq_id> id(new CSeq_id);
            id->Assign(*idh.GetSeqId());
            TRangeMap& rmap = IsReverse(it.GetStrand()) ?
                id_map_minus[idh] : id_map_plus[idh];
            x_AddNonOverlappingRangeToLoc(res,
                                          *id,
                                          rg,
                                          it.GetStrand(),
                                          rmap);
            rmap += it.GetRange();
        }
        else {
            // Sort resulting ranges
            TRangeMap& rmap = IsReverse(it.GetStrand()) ?
                id_map_minus[idh] : id_map_plus[idh];
            rmap += it.GetRange();
        }
    }
}


void x_IdMapToSeqLoc(TIdToRangeMap& id_map, CSeq_loc& res, ENa_strand strand)
{
    ITERATE(TIdToRangeMap, id_it, id_map) {
        CRef<CSeq_id> id(new CSeq_id);
        id->Assign(*id_it->first.GetSeqId());
        ITERATE(TRangeMap, rg_it, id_it->second) {
            x_AddRangeToLoc(res, *id, *rg_it, strand);
        }
    }
}


class CDefaultSynonymMapper : public CSynonymMapper_Base
{
public:
    CDefaultSynonymMapper(CScope* scope);
    virtual ~CDefaultSynonymMapper(void);

    virtual CSeq_id_Handle GetBestSynonym(const CSeq_id& id);

protected:
    typedef map<CSeq_id_Handle, CSeq_id_Handle> TSynonymMap;

    CRef<CSeq_id_Mapper> m_IdMapper;
    TSynonymMap          m_SynMap;
    CScope*              m_Scope;
};


CDefaultSynonymMapper::CDefaultSynonymMapper(CScope* scope)
    : m_IdMapper(CSeq_id_Mapper::GetInstance()),
      m_Scope(scope)
{
    return;
}


CDefaultSynonymMapper::~CDefaultSynonymMapper(void)
{
    return;
}


CSeq_id_Handle CDefaultSynonymMapper::GetBestSynonym(const CSeq_id& id)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id);
    if ( !m_Scope ) {
        return idh;
    }
    TSynonymMap::iterator id_syn = m_SynMap.find(idh);
    if (id_syn != m_SynMap.end()) {
        return id_syn->second;
    }
    CSeq_id_Handle best;
    int best_rank = kMax_Int;
    CConstRef<CSynonymsSet> syn_set = m_Scope->GetSynonyms(idh);
    ITERATE(CSynonymsSet, syn_it, *syn_set) {
        CSeq_id_Handle synh = syn_set->GetSeq_id_Handle(syn_it);
        int rank = synh.GetSeqId()->BestRankScore();
        if (rank < best_rank) {
            best = synh;
            best_rank = rank;
        }
    }
    if ( !best ) {
        // Synonyms set was empty
        m_SynMap[idh] = idh;
        return idh;
    }
    ITERATE(CSynonymsSet, syn_it, *syn_set) {
        m_SynMap[syn_set->GetSeq_id_Handle(syn_it)] = best;
    }
    return best;
}


class CDefaultLengthGetter : public CLengthGetter_Base
{
public:
    CDefaultLengthGetter(CScope* scope);
    virtual ~CDefaultLengthGetter(void);

    virtual TSeqPos GetLength(const CSeq_id& id);

protected:
    CScope*              m_Scope;
};


CDefaultLengthGetter::CDefaultLengthGetter(CScope* scope)
    : m_Scope(scope)
{
    return;
}


CDefaultLengthGetter::~CDefaultLengthGetter(void)
{
    return;
}


TSeqPos CDefaultLengthGetter::GetLength(const CSeq_id& id)
{
    CBioseq_Handle bh;
    if ( m_Scope ) {
        bh = m_Scope->GetBioseqHandle(id);
    }
    if ( !bh ) {
        NCBI_THROW(CException, eUnknown,
            "Can not get length of whole location");
    }
    return bh.GetBioseqLength();
}


CRef<CSeq_loc> x_Add(const CSeq_loc&      loc1,
                     const CSeq_loc&      loc2,
                     EMergeFlag           merge_flag,
                     EStrandFlag          strand_flag,
                     CSynonymMapper_Base& syn_map)
{
    _ASSERT(merge_flag != eNoMerge);

    CRef<CSeq_loc> res(new CSeq_loc);

    // Id -> range map for both strands
    auto_ptr<TIdToRangeMap> pid_map(new TIdToRangeMap);
    auto_ptr<TIdToRangeMap> pid_map_minus(strand_flag == ePreserveStrand ?
        new TIdToRangeMap : 0);
    TIdToRangeMap& id_map = *pid_map.get();
    TIdToRangeMap& id_map_minus = pid_map_minus.get() ?
        *pid_map_minus.get() : id_map;

    // Prepare default strands
    ENa_strand default_plus = strand_flag == ePreserveStrand ?
        eNa_strand_plus : eNa_strand_unknown;
    ENa_strand default_minus = strand_flag == ePreserveStrand ?
        eNa_strand_minus : eNa_strand_unknown;

    x_ProcessSeqLoc(loc1, merge_flag, syn_map, 0, id_map, id_map_minus, *res);
    x_ProcessSeqLoc(loc2, merge_flag, syn_map, 0, id_map, id_map_minus, *res);

    if (merge_flag == eMergeAndSort) {
        if ( id_map_minus.empty() ) {
            // If there's no minus strand, use unknown rather than plus
            default_plus = eNa_strand_unknown;
        }
        x_IdMapToSeqLoc(id_map, *res, default_plus);
        if (strand_flag == ePreserveStrand) {
            // Process minus strand
            x_IdMapToSeqLoc(id_map_minus, *res, default_minus);
        }
    }
    return res;
}


CRef<CSeq_loc> Seq_loc_Add(const CSeq_loc&      loc1,
                           const CSeq_loc&      loc2,
                           EMergeFlag           merge_flag,
                           EStrandFlag          strand_flag,
                           CScope*              scope)
{
    CRef<CSeq_loc> res(new CSeq_loc);

    if (merge_flag == eNoMerge) {
        // Strand flag does not matter if there's no merging
        res->Assign(loc1);
        res->Add(loc2);
        if ( scope ) {
            // Replace IDs with their best synonyms
            ChangeSeqLocId(res.GetPointer(), true, scope);
        }
        return res;
    }

    // Resolving synonyms and selecting the best one
    CDefaultSynonymMapper syn_map(scope);

    return x_Add(loc1, loc2, merge_flag, strand_flag, syn_map);
}


CRef<CSeq_loc> Seq_loc_Add(const CSeq_loc&      loc1,
                           const CSeq_loc&      loc2,
                           CSynonymMapper_Base& syn_mapper,
                           EMergeFlag           merge_flag,
                           EStrandFlag          strand_flag)
{
    CRef<CSeq_loc> res(new CSeq_loc);

    if (merge_flag == eNoMerge) {
        // Strand flag does not matter if there's no merging
        res->Assign(loc1);
        res->Add(loc2);
        // Replace IDs with their best synonyms
        for (CTypeIterator<CSeq_id> id(Begin(*res)); id; ++id) {
            CSeq_id_Handle idh = syn_mapper.GetBestSynonym(*id);
            if ( !id->Equals(*idh.GetSeqId()) ) {
                id->Reset();
                id->Assign(*idh.GetSeqId());
            }
        }
        return res;
    }

    return x_Add(loc1, loc2, merge_flag, strand_flag, syn_mapper);
}


CRef<CSeq_loc> Seq_loc_Subtract(const CSeq_loc&      loc1,
                                const CSeq_loc&      loc2,
                                CSynonymMapper_Base& syn_mapper,
                                CLengthGetter_Base&  len_getter,
                                EMergeFlag           merge_flag,
                                EStrandFlag          strand_flag)
{
    CRef<CSeq_loc> res(new CSeq_loc);

    // Id -> range map for both strands, first seq-loc
    auto_ptr<TIdToRangeMap> pid_map_plus1(new TIdToRangeMap);
    auto_ptr<TIdToRangeMap> pid_map_minus1(strand_flag == ePreserveStrand ?
        new TIdToRangeMap : 0);
    TIdToRangeMap& id_map_plus1 = *pid_map_plus1.get();
    TIdToRangeMap& id_map_minus1 = pid_map_minus1.get() ?
        *pid_map_minus1.get() : id_map_plus1;

    // Id -> range map for both strands, second seq-loc
    auto_ptr<TIdToRangeMap> pid_map_plus2(new TIdToRangeMap);
    auto_ptr<TIdToRangeMap> pid_map_minus2(strand_flag == ePreserveStrand ?
        new TIdToRangeMap : 0);
    TIdToRangeMap& id_map_plus2 = *pid_map_plus2.get();
    TIdToRangeMap& id_map_minus2 = pid_map_minus2.get() ?
        *pid_map_minus2.get() : id_map_plus2;

    // Prepare default strands
    ENa_strand default_plus = strand_flag == ePreserveStrand ?
        eNa_strand_plus : eNa_strand_unknown;
    ENa_strand default_minus = strand_flag == ePreserveStrand ?
        eNa_strand_minus : eNa_strand_unknown;

    // Collect ranges from the second seq-loc, nothing should be
    // added to the destination.
    x_ProcessSeqLoc(loc2,
                    eMergeAndSort,
                    syn_mapper,
                    &len_getter,
                    id_map_plus2,
                    id_map_minus2,
                    *res);

    if (merge_flag == eMergeAndSort) {
        x_ProcessSeqLoc(loc1,
                        eMergeAndSort,
                        syn_mapper,
                        &len_getter,
                        id_map_plus1,
                        id_map_minus1,
                        *res);
        NON_CONST_ITERATE(TIdToRangeMap, id_it, id_map_plus1) {
            TIdToRangeMap::iterator id2_it = id_map_plus2.find(id_it->first);
            if (id2_it != id_map_plus2.end()) {
                id_it->second -= id2_it->second;
            }
        }
        if (strand_flag == ePreserveStrand  &&  !id_map_minus2.empty()) {
            NON_CONST_ITERATE(TIdToRangeMap, id_it, id_map_minus1) {
                TIdToRangeMap::iterator id2_it = id_map_minus2.find(id_it->first);
                if (id2_it != id_map_minus2.end()) {
                    id_it->second -= id2_it->second;
                }
            }
        }
        else {
            default_plus = eNa_strand_unknown;
        }
    }
    else {
        for (CSeq_loc_CI it(loc1); it; ++it) {
            // Prepare source range
            TRange rg = it.GetRange();
            if ( it.IsWhole() ) {
                rg.SetOpen(0, len_getter.GetLength(it.GetSeq_id()));
            }

            CSeq_id_Handle idh = syn_mapper.GetBestSynonym(it.GetSeq_id());
            CRef<CSeq_id> id(new CSeq_id);
            id->Assign(*idh.GetSeqId());

            if (merge_flag == eNoMerge) {
                TRangeMap diff_map(rg);
                TRangeMap& rmap = strand_flag == eIgnoreStrand ||
                    !IsReverse(it.GetStrand()) ?
                    id_map_plus2[idh] : id_map_minus2[idh];
                diff_map -= rmap;
                const TRangeMap& const_diff_map = diff_map;
                ITERATE(TRangeMap, rg_it, const_diff_map) {
                    x_AddRangeToLoc(*res, *id, *rg_it, it.GetStrand());
                }
            }
            else if (merge_flag == eMerge) {
                // Fill the destination seq-loc
                TRangeMap& rmap = IsReverse(it.GetStrand()) ?
                    id_map_minus2[idh] : id_map_plus2[idh];
                x_AddNonOverlappingRangeToLoc(*res,
                                            *id,
                                            rg,
                                            it.GetStrand(),
                                            rmap);
                rmap += it.GetRange();
            }
        }
    }

    if (merge_flag == eMergeAndSort) {
        if ( id_map_minus1.empty() ) {
            // If there's no minus strand, use unknown rather than plus
            default_plus = eNa_strand_unknown;
        }
        x_IdMapToSeqLoc(id_map_plus1, *res, default_plus);
        if (strand_flag == ePreserveStrand) {
            // Process minus strand
            x_IdMapToSeqLoc(id_map_minus1, *res, default_minus);
        }
    }

    return res;
}


CRef<CSeq_loc> Seq_loc_Subtract(const CSeq_loc& loc1,
                                const CSeq_loc& loc2,
                                EMergeFlag      merge_flag,
                                EStrandFlag     strand_flag,
                                CScope* scope)
{
    // Resolving synonyms and selecting the best one
    CDefaultSynonymMapper syn_mapper(scope);
    CDefaultLengthGetter len_getter(scope);
    return Seq_loc_Subtract(loc1, loc2,
                            syn_mapper, len_getter,
                            merge_flag, strand_flag);
}


CRef<CSeq_loc> Seq_loc_Merge(const CSeq_loc& loc,
                             CSynonymMapper_Base& syn_mapper,
                             EMergeFlag  merge_flag,
                             EStrandFlag strand_flag)
{
    CRef<CSeq_loc> res(new CSeq_loc);
    if (merge_flag == eNoMerge) {
        res->Assign(loc);
        return res;
    }
    // Id -> range map for both strands
    auto_ptr<TIdToRangeMap> pid_map_plus(new TIdToRangeMap);
    auto_ptr<TIdToRangeMap> pid_map_minus(strand_flag == ePreserveStrand ?
        new TIdToRangeMap : 0);
    TIdToRangeMap& id_map_plus = *pid_map_plus.get();
    TIdToRangeMap& id_map_minus = pid_map_minus.get() ?
        *pid_map_minus.get() : id_map_plus;

    // Prepare default strands
    ENa_strand default_plus = strand_flag == ePreserveStrand ?
        eNa_strand_plus : eNa_strand_unknown;
    ENa_strand default_minus = strand_flag == ePreserveStrand ?
        eNa_strand_minus : eNa_strand_unknown;

    x_ProcessSeqLoc(loc, merge_flag, syn_mapper, 0,
        id_map_plus, id_map_minus, *res);

    if (merge_flag == eMergeAndSort) {
        if ( id_map_minus.empty() ) {
            // If there's no minus strand, use unknown rather than plus
            default_plus = eNa_strand_unknown;
        }
        x_IdMapToSeqLoc(id_map_plus, *res, default_plus);
        if (strand_flag == ePreserveStrand) {
            // Process minus strand
            x_IdMapToSeqLoc(id_map_minus, *res, default_minus);
        }
    }

    return res;
}


CRef<CSeq_loc> Seq_loc_Merge(const CSeq_loc& loc,
                             EMergeFlag  merge_flag,
                             EStrandFlag strand_flag,
                             CScope* scope)
{
    // Resolving synonyms and selecting the best one
    CDefaultSynonymMapper syn_mapper(scope);
    return Seq_loc_Merge(loc, syn_mapper, merge_flag, strand_flag);
}


END_SCOPE(sequence)
END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.1  2004/10/20 18:09:43  grichenk
* Initial revision
*
*
* ===========================================================================
*/
