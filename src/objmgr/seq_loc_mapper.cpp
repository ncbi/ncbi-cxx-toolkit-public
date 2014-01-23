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
    x_InitializeSeqMap(target_seq.GetSeqMap(),
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
    selector.SetLinkUsedTSE(target_seq.GetTSE_Handle());
    x_InitializeSeqMap(target_seq.GetSeqMap(), selector, top_id, direction);
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
    x_InitializeSeqMap(seq_map,
                       selector,
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
        x_InitializeSeqMap(top_level_seq.GetSeqMap(),
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
                                 CScope*             scope,
                                 EScopeFlag          scope_flag)
    : CSeq_loc_Mapper_Base(new CScope_Mapper_Sequence_Info(scope)),
      m_Scope(scope)
{
    // While parsing GC-Assembly the mapper will need to add virtual
    // bioseqs to the scope. To keep the original scope clean of them,
    // create a new scope and add the original one as a child.
    if (scope_flag == eCopyScope) {
        m_Scope = CHeapScope(new CScope(*CObjectManager::GetInstance()));
        if ( scope ) {
            m_Scope.GetScope().AddScope(*scope);
        }
        m_SeqInfo.Reset(new CScope_Mapper_Sequence_Info(m_Scope));
    }
    x_InitGCAssembly(gc_assembly, to_alias);
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CGC_Assembly& gc_assembly,
                                 ESeqMapDirection    direction,
                                 SSeqMapSelector     selector,
                                 CScope*             scope,
                                 EScopeFlag          scope_flag)
    : CSeq_loc_Mapper_Base(new CScope_Mapper_Sequence_Info(scope)),
      m_Scope(scope)
{
    // While parsing GC-Assembly the mapper will need to add virtual
    // bioseqs to the scope. To keep the original scope clean of them,
    // create a new scope and add the original one as a child.
    if (scope_flag == eCopyScope) {
        m_Scope = CHeapScope(new CScope(*CObjectManager::GetInstance()));
        if ( scope ) {
            m_Scope.GetScope().AddScope(*scope);
        }
        m_SeqInfo.Reset(new CScope_Mapper_Sequence_Info(m_Scope));
    }
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


void CSeq_loc_Mapper::x_InitializeSeqMap(const CSeqMap&   seq_map,
                                         size_t           depth,
                                         const CSeq_id*   top_id,
                                         ESeqMapDirection direction)
{
    x_InitializeSeqMap(seq_map,
                       SSeqMapSelector(0, depth),
                       top_id,
                       direction);
}


void CSeq_loc_Mapper::x_InitializeSeqMap(const CSeqMap&   seq_map,
                                         SSeqMapSelector  selector,
                                         const CSeq_id*   top_id,
                                         ESeqMapDirection direction)
{
    selector.SetFlags(CSeqMap::fFindRef | CSeqMap::fIgnoreUnresolved)
        .SetLinkUsedTSE();
    x_InitializeSeqMap(CSeqMap_CI(ConstRef(&seq_map),
                       m_Scope.GetScopeOrNull(), selector),
                       top_id,
                       direction);
}


void CSeq_loc_Mapper::x_InitializeSeqMap(CSeqMap_CI       seg_it,
                                         const CSeq_id*   top_id,
                                         ESeqMapDirection direction)
{
    TSeqPos src_from, src_len, dst_from, dst_len;
    ENa_strand dst_strand = eNa_strand_unknown;
    if (direction == eSeqMap_Up) {
        // Mapping up - for each iterator create mapping to the top level.
        // If top_id is set, top level positions are the iterator's positions.
        // Otherwise top-level ids and positions must be taked from the
        // iterators with depth == 1.
        TSeqPos top_ref_start = 0;
        TSeqPos top_start = 0;
        TSeqPos top_end = 0;
        CConstRef<CSeq_id> dst_id(top_id);
        _ASSERT(seg_it.GetDepth() == 1);
        while ( seg_it ) {
            ENa_strand src_strand = seg_it.GetRefMinusStrand() ? eNa_strand_minus : eNa_strand_plus;
            src_from = seg_it.GetRefPosition();
            src_len = seg_it.GetLength();
            dst_len = src_len;
            if (top_id) {
                dst_from = seg_it.GetPosition();
                x_NextMappingRange(
                    *seg_it.GetRefSeqid().GetSeqId(),
                    src_from, src_len, src_strand,
                    *top_id,
                    dst_from, dst_len, dst_strand);
            }
            else /* !top_id */ {
                if (seg_it.GetDepth() == 1) {
                    // Depth==1 - top level sequences (destination).
                    dst_id.Reset(seg_it.GetRefSeqid().GetSeqId());
                    top_ref_start = seg_it.GetRefPosition();
                    top_start = seg_it.GetPosition();
                    top_end = seg_it.GetEndPosition();
                    dst_strand = seg_it.GetRefMinusStrand() ? eNa_strand_minus : eNa_strand_plus;
                }
                else {
                    _ASSERT(seg_it.GetPosition() >= top_start);
                    _ASSERT(seg_it.GetEndPosition() <= top_end);
                    TSeqPos shift = seg_it.GetPosition() - top_start;
                    dst_from = top_ref_start + shift;
                    x_NextMappingRange(
                        *seg_it.GetRefSeqid().GetSeqId(),
                        src_from, src_len, src_strand,
                        *dst_id,
                        dst_from, dst_len, dst_strand);
                }
            }
            ++seg_it;
        }
        return; // Done with eSeqMap_Up
    }
    // eSeqMap_Down
    // Collect all non-leaf references, create mapping from each non-leaf
    // to the bottom level.
    list<CSeqMap_CI> refs;
    refs.push_back(seg_it);
    while ( seg_it ) {
        ++seg_it;
        // While depth increases push all iterators to the stack.
        if ( seg_it ) {
            if (refs.empty()  ||  refs.back().GetDepth() < seg_it.GetDepth()) {
                refs.push_back(seg_it);
                continue;
            }
        }
        // End of seq-map or the last iterator was a leaf - create mappings.
        if ( !refs.empty() ) {
            CSeqMap_CI leaf = refs.back();
            // Exclude self-mapping of the leaf reference.
            refs.pop_back();
            dst_strand = leaf.GetRefMinusStrand() ? eNa_strand_minus : eNa_strand_plus;
            if ( top_id ) {
                // Add mapping from the top-level sequence if any.
                src_from = leaf.GetPosition();
                src_len = leaf.GetLength();
                dst_from = leaf.GetRefPosition();
                dst_len = src_len;
                x_NextMappingRange(
                    *top_id,
                    src_from, src_len, eNa_strand_unknown,
                    *leaf.GetRefSeqid().GetSeqId(),
                    dst_from, dst_len, dst_strand);
            }
            // Create mapping from each non-leaf level.
            ITERATE(list<CSeqMap_CI>, it, refs) {
                TSeqPos shift = leaf.GetPosition() - it->GetPosition();
                ENa_strand src_strand = it->GetRefMinusStrand() ? eNa_strand_minus : eNa_strand_plus;
                src_from = it->GetRefPosition() + shift;
                src_len = leaf.GetLength();
                dst_from = leaf.GetRefPosition();
                dst_len = src_len;
                x_NextMappingRange(
                    *it->GetRefSeqid().GetSeqId(),
                    src_from, src_len, src_strand,
                    *leaf.GetRefSeqid().GetSeqId(),
                    dst_from, dst_len, dst_strand);
            }
            if ( seg_it ) {
                while ( !refs.empty()  &&  refs.back().GetDepth() >= seg_it.GetDepth()) {
                    refs.pop_back();
                }
                refs.push_back(seg_it);
            }
        }
    }
}


CBioseq_Handle
CSeq_loc_Mapper::x_AddVirtualBioseq(const TSynonyms&  synonyms,
                                    const CDelta_ext* delta)
{
    CRef<CBioseq> bioseq(new CBioseq);
    ITERATE(IMapper_Sequence_Info::TSynonyms, syn, synonyms) {
        if (!delta ) {
            CBioseq_Handle h = m_Scope.GetScope().GetBioseqHandle(*syn);
            if ( h ) {
                return h;
            }
        }
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


inline bool s_IsLocalRandomChrId(const CSeq_id& id)
{
    return id.IsLocal()  &&  id.GetLocal().IsStr()  &&
        id.GetLocal().GetStr().find("_random") != string::npos;
}


bool CSeq_loc_Mapper::x_IsUCSCRandomChr(const CGC_Sequence& gc_seq,
                                        CConstRef<CSeq_id>& chr_id,
                                        TSynonyms& synonyms) const
{
    chr_id.Reset();
    if (!gc_seq.IsSetStructure()) return false;

    CConstRef<CSeq_id> id(&gc_seq.GetSeq_id());
    synonyms.insert(CSeq_id_Handle::GetHandle(*id));
    if ( s_IsLocalRandomChrId(*id) ) {
        chr_id = id;
    }
    // Collect all synonyms.
    ITERATE(CGC_Sequence::TSeq_id_synonyms, it, gc_seq.GetSeq_id_synonyms()) {
        const CGC_TypedSeqId& gc_id = **it;
        switch ( gc_id.Which() ) {
        case CGC_TypedSeqId::e_Genbank:
            id.Reset(&gc_id.GetGenbank().GetGi());
            break;
        case CGC_TypedSeqId::e_Refseq:
            id.Reset(&gc_id.GetRefseq().GetGi());
            break;
        case CGC_TypedSeqId::e_External:
            id.Reset(&gc_id.GetExternal().GetId());
            break;
        case CGC_TypedSeqId::e_Private:
            id.Reset(&gc_id.GetPrivate());
            break;
        default:
            continue;
        }
        synonyms.insert(CSeq_id_Handle::GetHandle(*id));
        if ( !chr_id  &&  s_IsLocalRandomChrId(*id) ) {
            chr_id = id;
        }
    }

    if ( !chr_id ) {
        synonyms.clear();
        return false;
    }

    // Use only random chromosome ids, ignore other synonyms (?)
    string lcl_str = chr_id->GetLocal().GetStr();
    if ( !NStr::StartsWith(lcl_str, "chr") ) {
        CSeq_id lcl;
        lcl.SetLocal().SetStr("chr" + lcl_str);
        synonyms.insert(CSeq_id_Handle::GetHandle(lcl));
    }

    return true;
}


const CSeq_id* s_GetSeqIdAlias(const CGC_TypedSeqId& id,
                               CSeq_loc_Mapper::EGCAssemblyAlias alias)
{
    switch ( id.Which() ) {
    case CGC_TypedSeqId::e_Genbank:
        if (alias == CSeq_loc_Mapper::eGCA_Genbank) {
            return id.GetGenbank().IsSetGi() ?
                &id.GetGenbank().GetGi() : &id.GetGenbank().GetPublic();
        }
        if (alias == CSeq_loc_Mapper::eGCA_GenbankAcc) {
            return &id.GetGenbank().GetPublic();
        }
        break;
    case CGC_TypedSeqId::e_Refseq:
        if (alias == CSeq_loc_Mapper::eGCA_Refseq) {
            return id.GetRefseq().IsSetGi() ?
                &id.GetRefseq().GetGi() : &id.GetRefseq().GetPublic();
        }
        if (alias == CSeq_loc_Mapper::eGCA_RefseqAcc) {
            return &id.GetRefseq().GetPublic();
        }
        break;
    case CGC_TypedSeqId::e_External:
        if (alias == CSeq_loc_Mapper::eGCA_UCSC  &&
            id.GetExternal().GetExternal() == "UCSC") {
            return &id.GetExternal().GetId();
        }
        break;
    case CGC_TypedSeqId::e_Private:
        if (alias == CSeq_loc_Mapper::eGCA_Other) {
            return &id.GetPrivate();
        }
        break;
    default:
        break;
    }
    return 0;
}


void CSeq_loc_Mapper::x_InitGCSequence(const CGC_Sequence& gc_seq,
                                       EGCAssemblyAlias    to_alias)
{
    if ( gc_seq.IsSetSeq_id_synonyms() ) {
        CConstRef<CSeq_id> dst_id;
        ITERATE(CGC_Sequence::TSeq_id_synonyms, it, gc_seq.GetSeq_id_synonyms()) {
            const CGC_TypedSeqId& id = **it;
            dst_id.Reset(s_GetSeqIdAlias(id, to_alias));
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
                    if (dst_id != &id.GetGenbank().GetGi()) {
                        synonyms.insert(CSeq_id_Handle::GetHandle(id.GetGenbank().GetGi()));
                    }
                    if (dst_id != &id.GetGenbank().GetPublic()) {
                        synonyms.insert(CSeq_id_Handle::GetHandle(id.GetGenbank().GetPublic()));
                    }
                    if ( id.GetGenbank().IsSetGpipe() ) {
                        synonyms.insert(CSeq_id_Handle::GetHandle(id.GetGenbank().GetGpipe()));
                    }
                    break;
                case CGC_TypedSeqId::e_Refseq:
                    if (dst_id != &id.GetRefseq().GetGi()) {
                        synonyms.insert(CSeq_id_Handle::GetHandle(id.GetRefseq().GetGi()));
                    }
                    if (dst_id != &id.GetRefseq().GetPublic()) {
                        synonyms.insert(CSeq_id_Handle::GetHandle(id.GetRefseq().GetPublic()));
                    }
                    if ( id.GetRefseq().IsSetGpipe() ) {
                        synonyms.insert(CSeq_id_Handle::GetHandle(id.GetRefseq().GetGpipe()));
                    }
                    break;
                case CGC_TypedSeqId::e_Private:
                    // Ignore private local ids - they are not unique.
                    if (id.GetPrivate().IsLocal()) continue;
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
            CConstRef<CSeq_id> chr_id;
            // The requested alias type not found,
            // check for UCSC random chromosomes.
            if ( x_IsUCSCRandomChr(gc_seq, chr_id, synonyms) ) {
                _ASSERT(chr_id);
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
                                *chr_id, chr_pos, chr_len,
                                eNa_strand_unknown);
                            break;
                        case eGCA_Refseq:
                            // Map down to delta parts
                            x_NextMappingRange(*chr_id, chr_pos, chr_len,
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
                        direction, selector, NULL, null);
                }
                else {
                    ITERATE(CGC_Replicon::TSequence::TSet, it, seq.GetSet()) {
                        x_InitGCSequence(**it,
                            direction, selector, NULL, null);
                    }
                }
            }
        }
        if ( unit.IsSetOther_sequences() ) {
            ITERATE(CGC_Sequence::TSequences, seq, unit.GetOther_sequences()) {
                ITERATE(CGC_TaggedSequences::TSeqs, tseq, (*seq)->GetSeqs()) {
                    x_InitGCSequence(**tseq, direction, selector, NULL, null);
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
                                       const CGC_Sequence* parent_seq,
                                       CRef<CSeq_id>       override_id)
{
    CRef<CSeq_id> id(override_id);
    if ( !id ) {
        id.Reset(new CSeq_id);
        id->Assign(gc_seq.GetSeq_id());
    }

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
    synonyms.insert(CSeq_id_Handle::GetHandle(*id));
    if ( gc_seq.IsSetSeq_id_synonyms() ) {
        ITERATE(CGC_Sequence::TSeq_id_synonyms, it, gc_seq.GetSeq_id_synonyms()) {
            // Add conversion for each synonym which can be used
            // as a source id.
            const CGC_TypedSeqId& it_id = **it;
            switch ( it_id.Which() ) {
            case CGC_TypedSeqId::e_Genbank:
                synonyms.insert(CSeq_id_Handle::GetHandle(it_id.GetGenbank().GetPublic()));
                synonyms.insert(CSeq_id_Handle::GetHandle(it_id.GetGenbank().GetGi()));
                if ( it_id.GetGenbank().IsSetGpipe() ) {
                    synonyms.insert(CSeq_id_Handle::GetHandle(it_id.GetGenbank().GetGpipe()));
                }
                break;
            case CGC_TypedSeqId::e_Refseq:
            {
                // If some of the ids is used in the structure (see above),
                // ignore all refseq ids.
                synonyms.insert(CSeq_id_Handle::GetHandle(it_id.GetRefseq().GetPublic()));
                synonyms.insert(CSeq_id_Handle::GetHandle(it_id.GetRefseq().GetGi()));
                if (it_id.GetRefseq().IsSetGpipe()) {
                    synonyms.insert(CSeq_id_Handle::GetHandle(it_id.GetRefseq().GetGpipe()));
                }
                break;
            }
            case CGC_TypedSeqId::e_Private:
                // Ignore private local ids.
                if (it_id.GetPrivate().IsLocal()) continue;
                synonyms.insert(CSeq_id_Handle::GetHandle(it_id.GetPrivate()));
                break;
            case CGC_TypedSeqId::e_External:
                synonyms.insert(CSeq_id_Handle::GetHandle(it_id.GetExternal().GetId()));
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
                parent_seq,
                id);
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
                x_InitGCSequence(**tseq, direction, selector, parent, null);
            }
        }
    }
    if (gc_seq.IsSetStructure()  &&  !parent_seq ) {
        // This is a top-level sequence or we are mapping down,
        // create CSeqMap.
        x_InitializeSeqMap(bh.GetSeqMap(), selector, id, direction);
        if (direction == eSeqMap_Up) {
            // Ignore seq-map destination ranges, map whole sequence to itself,
            // use unknown strand only.
            m_DstRanges.resize(1);
            m_DstRanges[0].clear();
            m_DstRanges[0][CSeq_id_Handle::GetHandle(*id)]
                .push_back(TRange::GetWhole());
        }
        x_PreserveDestinationLocs();
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
