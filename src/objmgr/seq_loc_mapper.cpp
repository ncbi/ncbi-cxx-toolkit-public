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
*   Seq-loc mapper
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/impl/seq_align_mapper.hpp>
#include <objmgr/impl/seq_loc_cvt.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/genomecoll/genome_collection__.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_gap.hpp>
#include <algorithm>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////
//
// CScope_Mapper_Sequence_Info
//
//   Sequence type/length/synonyms provider using CScope to fetch
//   the information.


class CScope_Mapper_Sequence_Info : public IMapper_Sequence_Info
{
public:
    CScope_Mapper_Sequence_Info(CScope* scope);

    virtual TSeqType GetSequenceType(const CSeq_id_Handle& idh);
    virtual TSeqPos GetSequenceLength(const CSeq_id_Handle& idh);
    virtual void CollectSynonyms(const CSeq_id_Handle& id,
                                 TSynonyms&            synonyms);
private:
    CHeapScope m_Scope;
};


CScope_Mapper_Sequence_Info::CScope_Mapper_Sequence_Info(CScope* scope)
    : m_Scope(scope)
{
}


void CScope_Mapper_Sequence_Info::
CollectSynonyms(const CSeq_id_Handle& id,
                TSynonyms&            synonyms)
{
    if ( m_Scope.IsNull() ) {
        synonyms.insert(id);
    }
    else {
        CConstRef<CSynonymsSet> syns =
            m_Scope.GetScope().GetSynonyms(id);
        ITERATE(CSynonymsSet, syn_it, *syns) {
            synonyms.insert(CSynonymsSet::GetSeq_id_Handle(syn_it));
        }
    }
}


CScope_Mapper_Sequence_Info::TSeqType
CScope_Mapper_Sequence_Info::GetSequenceType(const CSeq_id_Handle& idh)
{
    if ( m_Scope.IsNull() ) {
        return CSeq_loc_Mapper_Base::eSeq_unknown;
    }
    TSeqType seqtype = CSeq_loc_Mapper_Base::eSeq_unknown;
    CBioseq_Handle handle;
    try {
        handle = m_Scope.GetScope().GetBioseqHandle(idh);
        if ( handle ) {
            switch ( handle.GetBioseqMolType() ) {
            case CSeq_inst::eMol_dna:
            case CSeq_inst::eMol_rna:
            case CSeq_inst::eMol_na:
                seqtype = CSeq_loc_Mapper_Base::eSeq_nuc;
                break;
            case CSeq_inst::eMol_aa:
                seqtype = CSeq_loc_Mapper_Base::eSeq_prot;
                break;
            default:
                break;
            }
        }
    }
    catch ( exception& ) {
    }
    return seqtype;
}


TSeqPos CScope_Mapper_Sequence_Info::GetSequenceLength(const CSeq_id_Handle& idh)
{
    CBioseq_Handle h;
    if ( m_Scope.IsNull() ) {
        return kInvalidSeqPos;
    }
    h = m_Scope.GetScope().GetBioseqHandle(idh);
    if ( !h ) {
        NCBI_THROW(CAnnotMapperException, eUnknownLength,
                    "Can not get sequence length -- unknown seq-id");
    }
    return h.GetBioseqLength();
}


/////////////////////////////////////////////////////////////////////
//
// CSeq_loc_Mapper
//


/////////////////////////////////////////////////////////////////////
//
//   Initialization of the mapper
//


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


CSeq_loc_Mapper::CSeq_loc_Mapper(CMappingRanges* mapping_ranges,
                                 CScope*         scope)
    : CSeq_loc_Mapper_Base(mapping_ranges,
                           new CScope_Mapper_Sequence_Info(scope)),
      m_Scope(scope)
{
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CSeq_feat&  map_feat,
                                 EFeatMapDirection dir,
                                 CScope*           scope)
    : CSeq_loc_Mapper_Base(new CScope_Mapper_Sequence_Info(scope)),
      m_Scope(scope)
{
    x_InitializeFeat(map_feat, dir);
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CSeq_loc& source,
                                 const CSeq_loc& target,
                                 CScope* scope)
    : CSeq_loc_Mapper_Base(new CScope_Mapper_Sequence_Info(scope)),
      m_Scope(scope)
{
    x_InitializeLocs(source, target);
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CSeq_align& map_align,
                                 const CSeq_id&    to_id,
                                 CScope*           scope,
                                 TMapOptions       opts)
    : CSeq_loc_Mapper_Base(new CScope_Mapper_Sequence_Info(scope)),
      m_Scope(scope)
{
    x_InitializeAlign(map_align, to_id, opts);
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CSeq_align& map_align,
                                 size_t            to_row,
                                 CScope*           scope,
                                 TMapOptions       opts)
    : CSeq_loc_Mapper_Base(new CScope_Mapper_Sequence_Info(scope)),
      m_Scope(scope)
{
    x_InitializeAlign(map_align, to_row, opts);
}


CSeq_loc_Mapper::CSeq_loc_Mapper(CBioseq_Handle target_seq,
                                 ESeqMapDirection direction)
    : CSeq_loc_Mapper_Base(new CScope_Mapper_Sequence_Info(
                           &target_seq.GetScope())),
      m_Scope(&target_seq.GetScope())
{
    CConstRef<CSeq_id> top_level_id = target_seq.GetSeqId();
    if ( !top_level_id ) {
        // Bioseq handle has no id, try to get one.
        CConstRef<CSynonymsSet> syns = target_seq.GetSynonyms();
        if ( !syns->empty() ) {
            top_level_id = syns->GetSeq_id_Handle(syns->begin()).GetSeqId();
        }
    }
    x_InitializeBioseq(target_seq,
                       top_level_id.GetPointerOrNull(),
                       direction);
    if (direction == eSeqMap_Up) {
        // Ignore seq-map destination ranges, map whole sequence to itself,
        // use unknown strand only.
        m_DstRanges.resize(1);
        m_DstRanges[0].clear();
        m_DstRanges[0][CSeq_id_Handle::GetHandle(*top_level_id)]
            .push_back(TRange::GetWhole());
    }
    x_PreserveDestinationLocs();
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CSeqMap&   seq_map,
                                 ESeqMapDirection direction,
                                 const CSeq_id*   top_level_id,
                                 CScope*          scope)
    : CSeq_loc_Mapper_Base(new CScope_Mapper_Sequence_Info(scope)),
      m_Scope(scope)
{
    x_InitializeSeqMap(seq_map, top_level_id, direction);
    x_PreserveDestinationLocs();
}


CSeq_loc_Mapper::CSeq_loc_Mapper(CBioseq_Handle   target_seq,
                                 ESeqMapDirection direction,
                                 SSeqMapSelector  selector)
    : CSeq_loc_Mapper_Base(new CScope_Mapper_Sequence_Info(
                           &target_seq.GetScope())),
      m_Scope(&target_seq.GetScope())
{
    CConstRef<CSeq_id> top_id = target_seq.GetSeqId();
    if ( !top_id ) {
        // Bioseq handle has no id, try to get one.
        CConstRef<CSynonymsSet> syns = target_seq.GetSynonyms();
        if ( !syns->empty() ) {
            top_id = syns->GetSeq_id_Handle(syns->begin()).GetSeqId();
        }
    }
    selector.SetFlags(CSeqMap::fFindRef | CSeqMap::fIgnoreUnresolved)
        .SetLinkUsedTSE();
    x_InitializeSeqMap(CSeqMap_CI(target_seq, selector), top_id, direction);
    if (direction == eSeqMap_Up) {
        // Ignore seq-map destination ranges, map whole sequence to itself,
        // use unknown strand only.
        m_DstRanges.resize(1);
        m_DstRanges[0].clear();
        m_DstRanges[0][CSeq_id_Handle::GetHandle(*top_id)]
            .push_back(TRange::GetWhole());
    }
    x_PreserveDestinationLocs();
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CSeqMap&   seq_map,
                                 ESeqMapDirection direction,
                                 SSeqMapSelector  selector,
                                 const CSeq_id*   top_level_id,
                                 CScope*          scope)
    : CSeq_loc_Mapper_Base(new CScope_Mapper_Sequence_Info(scope)),
      m_Scope(scope)
{
    selector.SetFlags(CSeqMap::fFindRef | CSeqMap::fIgnoreUnresolved)
        .SetLinkUsedTSE();
    x_InitializeSeqMap(CSeqMap_CI(ConstRef(&seq_map),
                       m_Scope.GetScopeOrNull(), selector),
                       top_level_id,
                       direction);
    x_PreserveDestinationLocs();
}


CSeq_loc_Mapper::CSeq_loc_Mapper(size_t                 depth,
                                 const CBioseq_Handle&  top_level_seq,
                                 ESeqMapDirection       direction)
    : CSeq_loc_Mapper_Base(new CScope_Mapper_Sequence_Info(
                           &top_level_seq.GetScope())),
      m_Scope(&top_level_seq.GetScope())
{
    if (depth > 0) {
        depth--;
        x_InitializeBioseq(top_level_seq,
                           depth,
                           top_level_seq.GetSeqId().GetPointer(),
                           direction);
    }
    else if (direction == eSeqMap_Up) {
        // Synonyms conversion
        CConstRef<CSeq_id> top_level_id = top_level_seq.GetSeqId();
        m_DstRanges.resize(1);
        m_DstRanges[0][CSeq_id_Handle::GetHandle(*top_level_id)]
            .push_back(TRange::GetWhole());
    }
    x_PreserveDestinationLocs();
}


CSeq_loc_Mapper::CSeq_loc_Mapper(size_t           depth,
                                 const CSeqMap&   top_level_seq,
                                 ESeqMapDirection direction,
                                 const CSeq_id*   top_level_id,
                                 CScope*          scope)
    : CSeq_loc_Mapper_Base(new CScope_Mapper_Sequence_Info(scope)),
      m_Scope(scope)
{
    if (depth > 0) {
        depth--;
        x_InitializeSeqMap(top_level_seq, depth, top_level_id, direction);
    }
    else if (direction == eSeqMap_Up) {
        // Synonyms conversion
        m_DstRanges.resize(1);
        m_DstRanges[0][CSeq_id_Handle::GetHandle(*top_level_id)]
            .push_back(TRange::GetWhole());
    }
    x_PreserveDestinationLocs();
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CGC_Assembly& gc_assembly,
                                 EGCAssemblyAlias    to_alias,
                                 CScope*             scope)
    : CSeq_loc_Mapper_Base(new CScope_Mapper_Sequence_Info(scope)),
      m_Scope(new CScope(*CObjectManager::GetInstance()))
{
    // While parsing GC-Assembly the mapper will need to add virtual
    // bioseqs to the scope. To keep the original scope clean of them,
    // create a new scope and add the original one as a child.
    if ( scope ) {
        m_Scope.GetScope().AddScope(*scope);
    }
    m_SeqInfo.Reset(new CScope_Mapper_Sequence_Info(m_Scope));
    x_InitGCAssembly(gc_assembly, to_alias);
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CGC_Assembly& gc_assembly,
                                 ESeqMapDirection    direction,
                                 SSeqMapSelector     selector,
                                 CScope*             scope)
    : CSeq_loc_Mapper_Base(new CScope_Mapper_Sequence_Info(scope)),
      m_Scope(new CScope(*CObjectManager::GetInstance()))
{
    // While parsing GC-Assembly the mapper will need to add virtual
    // bioseqs to the scope. To keep the original scope clean of them,
    // create a new scope and add the original one as a child.
    if ( scope ) {
        m_Scope.GetScope().AddScope(*scope);
    }
    m_SeqInfo.Reset(new CScope_Mapper_Sequence_Info(m_Scope));
    x_InitGCAssembly(gc_assembly, direction, selector);
}


CSeq_loc_Mapper::~CSeq_loc_Mapper(void)
{
    return;
}


void CSeq_loc_Mapper::x_InitializeSeqMap(const CSeqMap&   seq_map,
                                         const CSeq_id*   top_id,
                                         ESeqMapDirection direction)
{
    x_InitializeSeqMap(seq_map, size_t(-1), top_id, direction);
}


void CSeq_loc_Mapper::x_InitializeBioseq(const CBioseq_Handle& bioseq,
                                         const CSeq_id* top_id,
                                         ESeqMapDirection direction)
{
    x_InitializeBioseq(bioseq, size_t(-1), top_id, direction);
}


void CSeq_loc_Mapper::x_InitializeSeqMap(const CSeqMap&   seq_map,
                                         size_t           depth,
                                         const CSeq_id*   top_id,
                                         ESeqMapDirection direction)
{
    SSeqMapSelector sel(CSeqMap::fFindRef | CSeqMap::fIgnoreUnresolved, depth);
    sel.SetLinkUsedTSE();
    x_InitializeSeqMap(CSeqMap_CI(ConstRef(&seq_map),
                       m_Scope.GetScopeOrNull(), sel),
                       top_id,
                       direction);
}


void CSeq_loc_Mapper::x_InitializeBioseq(const CBioseq_Handle& bioseq,
                                         size_t                depth,
                                         const CSeq_id*        top_id,
                                         ESeqMapDirection      direction)
{
    x_InitializeSeqMap(CSeqMap_CI(bioseq, SSeqMapSelector(
        CSeqMap::fFindRef | CSeqMap::fIgnoreUnresolved, depth)),
        top_id,
        direction);
}


void CSeq_loc_Mapper::x_InitializeSeqMap(CSeqMap_CI       seg_it,
                                         const CSeq_id*   top_id,
                                         ESeqMapDirection direction)
{
    TSeqPos top_start = kInvalidSeqPos;
    TSeqPos top_stop = kInvalidSeqPos;
    TSeqPos dst_seg_start = kInvalidSeqPos;
    CConstRef<CSeq_id> dst_id;

    do {
        if ( !seg_it ) break;
        CSeqMap_CI prev_it = seg_it;
        ++seg_it;

        // When mapping down ignore non-leaf references.
        bool prev_is_leaf = !seg_it  ||
            seg_it.GetDepth() <= prev_it.GetDepth();
        if (direction == eSeqMap_Down  &&  !prev_is_leaf) continue;

        _ASSERT(prev_it.GetType() == CSeqMap::eSeqRef);
        if (prev_it.GetPosition() > top_stop  ||  !dst_id) {
            // New top-level segment
            top_start = prev_it.GetPosition();
            top_stop = prev_it.GetEndPosition() - 1;
            if (top_id) {
                // Top level is a bioseq
                dst_id.Reset(top_id);
                dst_seg_start = top_start;
            }
            else {
                // Top level is a seq-loc, positions are
                // on the first-level references
                dst_id = prev_it.GetRefSeqid().GetSeqId();
                dst_seg_start = prev_it.GetRefPosition();
                continue;
            }
        }
        // when top_id is set, destination position = GetPosition(),
        // else it needs to be calculated from top_start/stop and dst_start/stop.
        TSeqPos dst_from = dst_seg_start + prev_it.GetPosition() - top_start;
        _ASSERT(dst_from >= dst_seg_start);
        TSeqPos dst_len = prev_it.GetLength();
        CConstRef<CSeq_id> src_id(prev_it.GetRefSeqid().GetSeqId());
        TSeqPos src_from = prev_it.GetRefPosition();
        TSeqPos src_len = dst_len;
        ENa_strand src_strand = prev_it.GetRefMinusStrand() ?
            eNa_strand_minus : eNa_strand_unknown;
        switch (direction) {
        case eSeqMap_Up:
            x_NextMappingRange(*src_id, src_from, src_len, src_strand,
                               *dst_id, dst_from, dst_len, eNa_strand_unknown);
            break;
        case eSeqMap_Down:
            x_NextMappingRange(*dst_id, dst_from, dst_len, eNa_strand_unknown,
                               *src_id, src_from, src_len, src_strand);
            break;
        }
        _ASSERT(src_len == 0  &&  dst_len == 0);
    } while (seg_it);
}


CBioseq_Handle
CSeq_loc_Mapper::x_AddVirtualBioseq(const TSynonyms&  synonyms,
                                    const CDelta_ext* delta)
{
    CRef<CBioseq> bioseq(new CBioseq);
    ITERATE(IMapper_Sequence_Info::TSynonyms, syn, synonyms) {
        CRef<CSeq_id> syn_id(new CSeq_id);
        syn_id->Assign(*syn->GetSeqId());
        bioseq->SetId().push_back(syn_id);
    }
    bioseq->SetInst().SetMol(CSeq_inst::eMol_na);
    if ( delta ) {
        // Create delta sequence
        bioseq->SetInst().SetRepr(CSeq_inst::eRepr_delta);
        // const_cast should be safe here - we are not going to modify data
        bioseq->SetInst().SetExt().SetDelta(
            const_cast<CDelta_ext&>(*delta));
    }
    else {
        // Create virtual bioseq without length/data.
        bioseq->SetInst().SetRepr(CSeq_inst::eRepr_virtual);
    }
    return m_Scope.GetScope().AddBioseq(*bioseq);
}


void CSeq_loc_Mapper::x_InitGCAssembly(const CGC_Assembly& gc_assembly,
                                       EGCAssemblyAlias    to_alias)
{
    if ( gc_assembly.IsUnit() ) {
        const CGC_AssemblyUnit& unit = gc_assembly.GetUnit();
        if ( unit.IsSetMols() ) {
            ITERATE(CGC_AssemblyUnit::TMols, it, unit.GetMols()) {
                const CGC_Replicon::TSequence& seq = (*it)->GetSequence();
                if ( seq.IsSingle() ) {
                    x_InitGCSequence(seq.GetSingle(), to_alias);
                }
                else {
                    ITERATE(CGC_Replicon::TSequence::TSet, it, seq.GetSet()) {
                        x_InitGCSequence(**it, to_alias);
                    }
                }
            }
        }
        if ( unit.IsSetOther_sequences() ) {
            ITERATE(CGC_Sequence::TSequences, seq, unit.GetOther_sequences()) {
                ITERATE(CGC_TaggedSequences::TSeqs, tseq, (*seq)->GetSeqs()) {
                    x_InitGCSequence(**tseq, to_alias);
                }
            }
        }
    }
    else if ( gc_assembly.IsAssembly_set() ) {
        const CGC_AssemblySet& aset = gc_assembly.GetAssembly_set();
        x_InitGCAssembly(aset.GetPrimary_assembly(), to_alias);
        if ( aset.IsSetMore_assemblies() ) {
            ITERATE(CGC_AssemblySet::TMore_assemblies, assm, aset.GetMore_assemblies()) {
                x_InitGCAssembly(**assm, to_alias);
            }
        }
    }
}


void CSeq_loc_Mapper::x_InitGCSequence(const CGC_Sequence& gc_seq,
                                       EGCAssemblyAlias    to_alias)
{
    if ( gc_seq.IsSetSeq_id_synonyms() ) {
        CConstRef<CSeq_id> dst_id;
        ITERATE(CGC_Sequence::TSeq_id_synonyms, it, gc_seq.GetSeq_id_synonyms()) {
            const CGC_TypedSeqId& id = **it;
            switch ( id.Which() ) {
            case CGC_TypedSeqId::e_Genbank:
                if (to_alias == eGCA_Genbank) {
                    // Use GI rather than accession from 'public' member.
                    dst_id.Reset(&id.GetGenbank().GetGi());
                }
                break;
            case CGC_TypedSeqId::e_Refseq:
                if (to_alias == eGCA_Refseq) {
                    dst_id.Reset(&id.GetRefseq().GetGi());
                }
                break;
            case CGC_TypedSeqId::e_External:
                if (to_alias == eGCA_UCSC  &&
                    id.GetExternal().GetExternal() == "UCSC") {
                    dst_id.Reset(&id.GetExternal().GetId());
                }
                break;
            case CGC_TypedSeqId::e_Private:
                if (to_alias == eGCA_Other) {
                    dst_id.Reset(&id.GetPrivate());
                }
                break;
            default:
                break;
            }
            if ( dst_id ) break; // Use the first matching alias
        }
        if ( dst_id ) {
            TSynonyms synonyms;
            synonyms.insert(CSeq_id_Handle::GetHandle(gc_seq.GetSeq_id()));
            ITERATE(CGC_Sequence::TSeq_id_synonyms, it, gc_seq.GetSeq_id_synonyms()) {
                // Add conversion for each synonym which can be used
                // as a source id.
                const CGC_TypedSeqId& id = **it;
                switch ( id.Which() ) {
                case CGC_TypedSeqId::e_Genbank:
                    if (to_alias != eGCA_Genbank) {
                        synonyms.insert(CSeq_id_Handle::GetHandle(id.GetGenbank().GetPublic()));
                        synonyms.insert(CSeq_id_Handle::GetHandle(id.GetGenbank().GetGi()));
                        if ( id.GetGenbank().IsSetGpipe() ) {
                            synonyms.insert(CSeq_id_Handle::GetHandle(id.GetGenbank().GetGpipe()));
                        }
                    }
                    break;
                case CGC_TypedSeqId::e_Refseq:
                    if (to_alias != eGCA_Refseq) {
                        synonyms.insert(CSeq_id_Handle::GetHandle(id.GetRefseq().GetPublic()));
                        synonyms.insert(CSeq_id_Handle::GetHandle(id.GetRefseq().GetGi()));
                        if ( id.GetRefseq().IsSetGpipe() ) {
                            synonyms.insert(CSeq_id_Handle::GetHandle(id.GetRefseq().GetGpipe()));
                        }
                    }
                    break;
                case CGC_TypedSeqId::e_Private:
                    if (dst_id != &id.GetPrivate()) {
                        synonyms.insert(CSeq_id_Handle::GetHandle(id.GetPrivate()));
                    }
                    break;
                case CGC_TypedSeqId::e_External:
                    if (dst_id != &id.GetExternal().GetId()) {
                        synonyms.insert(CSeq_id_Handle::GetHandle(id.GetExternal().GetId()));
                    }
                    break;
                default:
                    NCBI_THROW(CAnnotMapperException, eOtherError,
                               "Unsupported alias type in GC-Sequence synonyms");
                    break;
                }
            }
            x_AddVirtualBioseq(synonyms);
            x_AddConversion(gc_seq.GetSeq_id(), 0, eNa_strand_unknown,
                *dst_id, 0, eNa_strand_unknown, TRange::GetWholeLength(),
                false, 0, kInvalidSeqPos, kInvalidSeqPos, kInvalidSeqPos );
        }
        else if (to_alias == eGCA_UCSC  ||  to_alias == eGCA_Refseq) {
            TSynonyms synonyms;
            // The requested alias type not found,
            // check for UCSC random chromosomes.
            const CSeq_id& id = gc_seq.GetSeq_id();
            if (gc_seq.IsSetStructure()  &&
                id.IsLocal()  &&  id.GetLocal().IsStr()  &&
                id.GetLocal().GetStr().find("_random") != string::npos) {

                string lcl_str = id.GetLocal().GetStr();
                CSeq_id lcl;
                lcl.SetLocal().SetStr(lcl_str);
                synonyms.insert(CSeq_id_Handle::GetHandle(lcl));
                if ( !NStr::StartsWith(lcl_str, "chr") ) {
                    lcl.SetLocal().SetStr("chr" + lcl_str);
                    synonyms.insert(CSeq_id_Handle::GetHandle(lcl));
                }
                // Ignore other synonyms - they will probably never be set. (?)
                x_AddVirtualBioseq(synonyms);

                // Use structure (delta-seq) to initialize the mapper.
                // Here we use just one level of the delta and parse it
                // directly rather than use CSeqMap.
                TSeqPos chr_pos = 0;
                TSeqPos chr_len = kInvalidSeqPos;
                ITERATE(CDelta_ext::Tdata, it, gc_seq.GetStructure().Get()) {
                    // Do not create mappings for literals/gaps.
                    if ( (*it)->IsLiteral() ) {
                        chr_pos += (*it)->GetLiteral().GetLength();
                    }
                    if ( !(*it)->IsLoc() ) {
                        continue;
                    }
                    CSeq_loc_CI loc_it((*it)->GetLoc());
                    for (; loc_it; ++loc_it) {
                        if ( loc_it.IsEmpty() ) continue;
                        TSeqPos seg_pos = loc_it.GetRange().GetFrom();
                        TSeqPos seg_len = loc_it.GetRange().GetLength();
                        ENa_strand seg_str = loc_it.IsSetStrand() ?
                            loc_it.GetStrand() : eNa_strand_unknown;
                        switch ( to_alias ) {
                        case eGCA_UCSC:
                            // Map up to the chr
                            x_NextMappingRange(loc_it.GetSeq_id(),
                                seg_pos, seg_len, seg_str,
                                id, chr_pos, chr_len,
                                eNa_strand_unknown);
                            break;
                        case eGCA_Refseq:
                            // Map down to delta parts
                            x_NextMappingRange(id, chr_pos, chr_len,
                                eNa_strand_unknown,
                                loc_it.GetSeq_id(), seg_pos, seg_len,
                                seg_str);
                            break;
                        default:
                            break;
                        }
                    }
                }
            }
        }
    }
    if ( gc_seq.IsSetSequences() ) {
        ITERATE(CGC_Sequence::TSequences, seq, gc_seq.GetSequences()) {
            ITERATE(CGC_TaggedSequences::TSeqs, tseq, (*seq)->GetSeqs()) {
                x_InitGCSequence(**tseq, to_alias);
            }
        }
    }
}


void CSeq_loc_Mapper::x_InitGCAssembly(const CGC_Assembly& gc_assembly,
                                       ESeqMapDirection    direction,
                                       SSeqMapSelector     selector)
{
    if ( gc_assembly.IsUnit() ) {
        const CGC_AssemblyUnit& unit = gc_assembly.GetUnit();
        if ( unit.IsSetMols() ) {
            ITERATE(CGC_AssemblyUnit::TMols, it, unit.GetMols()) {
                const CGC_Replicon::TSequence& seq = (*it)->GetSequence();
                if ( seq.IsSingle() ) {
                    x_InitGCSequence(seq.GetSingle(),
                        direction, selector, NULL);
                }
                else {
                    ITERATE(CGC_Replicon::TSequence::TSet, it, seq.GetSet()) {
                        x_InitGCSequence(**it,
                            direction, selector, NULL);
                    }
                }
            }
        }
        if ( unit.IsSetOther_sequences() ) {
            ITERATE(CGC_Sequence::TSequences, seq, unit.GetOther_sequences()) {
                ITERATE(CGC_TaggedSequences::TSeqs, tseq, (*seq)->GetSeqs()) {
                    x_InitGCSequence(**tseq, direction, selector, NULL);
                }
            }
        }
    }
    else if ( gc_assembly.IsAssembly_set() ) {
        const CGC_AssemblySet& aset = gc_assembly.GetAssembly_set();
        x_InitGCAssembly(aset.GetPrimary_assembly(), direction, selector);
        if ( aset.IsSetMore_assemblies() ) {
            ITERATE(CGC_AssemblySet::TMore_assemblies, assm,
                aset.GetMore_assemblies()) {
                x_InitGCAssembly(**assm, direction, selector);
            }
        }
    }
}


void CSeq_loc_Mapper::x_InitGCSequence(const CGC_Sequence& gc_seq,
                                       ESeqMapDirection    direction,
                                       SSeqMapSelector     selector,
                                       const CGC_Sequence* parent_seq)
{
    CRef<CSeq_id> id(new CSeq_id);
    id->Assign(gc_seq.GetSeq_id());

    // Special case - structure contains just one (whole) sequence and
    // the same sequence is mentioned in the synonyms. Must skip this
    // sequence and use the part instead.
    CSeq_id_Handle struct_syn;
    if ( gc_seq.IsSetStructure() ) {
        if (gc_seq.GetStructure().Get().size() == 1) {
            const CDelta_seq& delta = *gc_seq.GetStructure().Get().front();
            if ( delta.IsLoc() ) {
                const CSeq_loc& delta_loc = delta.GetLoc();
                switch (delta_loc.Which()) {
                case CSeq_loc::e_Whole:
                    struct_syn = CSeq_id_Handle::GetHandle(delta_loc.GetWhole());
                    break;
                case CSeq_loc::e_Int:
                    if (delta_loc.GetInt().GetFrom() == 0) {
                        struct_syn = CSeq_id_Handle::GetHandle(delta_loc.GetInt().GetId());
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }

    // Add synonyms if any.
    TSynonyms synonyms;
    if ( gc_seq.IsSetSeq_id_synonyms() ) {
        synonyms.insert(CSeq_id_Handle::GetHandle(*id));
        ITERATE(CGC_Sequence::TSeq_id_synonyms, it, gc_seq.GetSeq_id_synonyms()) {
            // Add conversion for each synonym which can be used
            // as a source id.
            const CGC_TypedSeqId& id = **it;
            switch ( id.Which() ) {
            case CGC_TypedSeqId::e_Genbank:
                synonyms.insert(CSeq_id_Handle::GetHandle(id.GetGenbank().GetPublic()));
                synonyms.insert(CSeq_id_Handle::GetHandle(id.GetGenbank().GetGi()));
                if ( id.GetGenbank().IsSetGpipe() ) {
                    synonyms.insert(CSeq_id_Handle::GetHandle(id.GetGenbank().GetGpipe()));
                }
                break;
            case CGC_TypedSeqId::e_Refseq:
            {
                // If some of the ids is used in the structure (see above),
                // ignore all refseq ids.
                synonyms.insert(CSeq_id_Handle::GetHandle(id.GetRefseq().GetPublic()));
                synonyms.insert(CSeq_id_Handle::GetHandle(id.GetRefseq().GetGi()));
                if (id.GetRefseq().IsSetGpipe()) {
                    synonyms.insert(CSeq_id_Handle::GetHandle(id.GetRefseq().GetGpipe()));
                }
                break;
            }
            case CGC_TypedSeqId::e_Private:
                synonyms.insert(CSeq_id_Handle::GetHandle(id.GetPrivate()));
                break;
            case CGC_TypedSeqId::e_External:
                synonyms.insert(CSeq_id_Handle::GetHandle(id.GetExternal().GetId()));
                break;
            default:
                NCBI_THROW(CAnnotMapperException, eOtherError,
                           "Unsupported alias type in GC-Sequence synonyms");
                break;
            }
        }
        // The sequence is referencing itself?
        if (synonyms.find(struct_syn) != synonyms.end()) {
            x_InitGCSequence(
                *gc_seq.GetSequences().front()->GetSeqs().front(),
                direction,
                selector,
                parent_seq);
            return;
        }
    }

    CBioseq_Handle bh;
    // Create virtual bioseq and use it to initialize the mapper
    if ( gc_seq.IsSetStructure() ) {
        bh = x_AddVirtualBioseq(synonyms, &gc_seq.GetStructure());
    }
    else {
        // Create literal sequence
        x_AddVirtualBioseq(synonyms);
    }

    if ( gc_seq.IsSetSequences() ) {
        ITERATE(CGC_Sequence::TSequences, seq, gc_seq.GetSequences()) {
            ITERATE(CGC_TaggedSequences::TSeqs, tseq, (*seq)->GetSeqs()) {
                // To create a sub-level of the existing seq-map we need
                // both structure at the current level and 'placed' state
                // on the child sequences. If this is not true, iterate
                // sub-sequences but treat them as top-level sequences rather
                // than segments.
                const CGC_Sequence* parent = 0;
                if (gc_seq.IsSetStructure()  &&
                    (*seq)->GetState() == CGC_TaggedSequences::eState_placed) {
                    parent = &gc_seq;
                }
                x_InitGCSequence(**tseq, direction, selector, parent);
            }
        }
    }
    if (gc_seq.IsSetStructure()  &&
        (!parent_seq  ||  direction == eSeqMap_Down)) {
        // This is a top-level sequence or we are mapping down,
        // create CSeqMap.
        SSeqMapSelector sel = selector;
        sel.SetFlags(CSeqMap::fFindRef | CSeqMap::fIgnoreUnresolved).
            SetLinkUsedTSE();
        x_InitializeSeqMap(CSeqMap_CI(bh, sel), id, direction);
        if (direction == eSeqMap_Up) {
            // Ignore seq-map destination ranges, map whole sequence to itself,
            // use unknown strand only.
            m_DstRanges.resize(1);
            m_DstRanges[0].clear();
            m_DstRanges[0][CSeq_id_Handle::GetHandle(*id)]
                .push_back(TRange::GetWhole());
            x_PreserveDestinationLocs();
        }
        else {
            m_DstRanges.clear();
        }
    }
}


/////////////////////////////////////////////////////////////////////
//
//   Initialization helpers
//


CSeq_align_Mapper_Base*
CSeq_loc_Mapper::InitAlignMapper(const CSeq_align& src_align)
{
    return new CSeq_align_Mapper(src_align, *this);
}


END_SCOPE(objects)
END_NCBI_SCOPE
