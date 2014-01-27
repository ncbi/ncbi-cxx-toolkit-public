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
* Authors:
*           Aleksey Grichenko
*
* File Description:
*           GC-Assembly parser used by CScope and CSeq_loc_Mapper to
*           convert assemblies to seq-entries.
*
*/

#include <ncbi_pch.hpp>

#include <objmgr/gc_assembly_parser.hpp>
#include <objmgr/error_codes.hpp>
#include <objects/genomecoll/genome_collection__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqset/seqset__.hpp>


#define NCBI_USE_ERRCODE_X   ObjMgr_GC_Assembly_Parser

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


const char* CAssemblyParserException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eUnsupported:      return "eUnsupported";
    case eOtherError:       return "eOtherError";
    default:                return CException::GetErrCodeString();
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// CGC_Assembly_Parser
//
/////////////////////////////////////////////////////////////////////////////


CGC_Assembly_Parser::CGC_Assembly_Parser(const CGC_Assembly& assembly,
                                         TParserFlags        flags)
    : m_Flags(flags)
{
    m_TSE.Reset(new CSeq_entry);
    x_InitSeq_entry(m_TSE, null);
    x_ParseGCAssembly(assembly, m_TSE);
}


CGC_Assembly_Parser::~CGC_Assembly_Parser(void)
{
}


void CGC_Assembly_Parser::x_InitSeq_entry(CRef<CSeq_entry> entry,
                                          CRef<CSeq_entry> parent)
{
    entry->SetSet().SetLevel(parent ? parent->GetSet().GetLevel() + 1 : 1);
    entry->SetSet().SetClass(CBioseq_set::eClass_segset);
    if (parent) {
        parent->SetSet().SetSeq_set().push_back(entry);
    }
}


void CGC_Assembly_Parser::x_CopyData(const CGC_AssemblyDesc& assm_desc,
                                       CSeq_entry&              entry)
{
    if (assm_desc.IsSetDescr()  &&  (m_Flags & fIgnoreDescr) == 0) {
        const CSeq_descr& descr = assm_desc.GetDescr();
        ITERATE(CSeq_descr::Tdata, desc, descr.Get()) {
            CRef<CSeqdesc> desc_copy(new CSeqdesc);
            desc_copy->Assign(**desc);
            entry.SetDescr().Set().push_back(desc_copy);
        }
    }
    if (assm_desc.IsSetAnnot()  &&  (m_Flags & fIgnoreAnnots) == 0) {
        ITERATE(CGC_AssemblyDesc::TAnnot, annot, assm_desc.GetAnnot()) {
            CRef<CSeq_annot> annot_copy(new CSeq_annot);
            annot_copy->Assign(**annot);
            entry.SetAnnot().push_back(annot_copy);
        }
    }
}


void CGC_Assembly_Parser::x_ParseGCAssembly(const CGC_Assembly& gc_assembly,
                                            CRef<CSeq_entry>    parent_entry)
{
    if ( gc_assembly.IsUnit() ) {
        const CGC_AssemblyUnit& unit = gc_assembly.GetUnit();
        if (unit.IsSetDesc()) {
            // Add annotations and descriptions.
            x_CopyData(unit.GetDesc(), *parent_entry);
        }
        if ( unit.IsSetMols() ) {
            ITERATE(CGC_AssemblyUnit::TMols, it, unit.GetMols()) {
                CRef<CSeq_entry> entry(new CSeq_entry);
                x_InitSeq_entry(entry, parent_entry);
                const CGC_Replicon::TSequence& seq = (*it)->GetSequence();
                if ( seq.IsSingle() ) {
                    x_ParseGCSequence(seq.GetSingle(), NULL, entry, null);
                }
                else {
                    ITERATE(CGC_Replicon::TSequence::TSet, it, seq.GetSet()) {
                        x_ParseGCSequence(**it, NULL, entry, null);
                    }
                }
            }
        }
        if ( unit.IsSetOther_sequences() ) {
            CRef<CSeq_entry> entry(new CSeq_entry);
            x_InitSeq_entry(entry, parent_entry);
            ITERATE(CGC_Sequence::TSequences, seq, unit.GetOther_sequences()) {
                ITERATE(CGC_TaggedSequences::TSeqs, tseq, (*seq)->GetSeqs()) {
                    x_ParseGCSequence(**tseq, NULL, entry, null);
                }
            }
        }
    }
    else if ( gc_assembly.IsAssembly_set() ) {
        const CGC_AssemblySet& aset = gc_assembly.GetAssembly_set();
        if (aset.IsSetDesc()) {
            // Add annotations and descriptions.
            x_CopyData(aset.GetDesc(), *parent_entry);
        }
        CRef<CSeq_entry> entry(new CSeq_entry);
        x_InitSeq_entry(entry, parent_entry);
        x_ParseGCAssembly(aset.GetPrimary_assembly(), entry);
        if ( aset.IsSetMore_assemblies() ) {
            ITERATE(CGC_AssemblySet::TMore_assemblies, assm,
                aset.GetMore_assemblies()) {
                x_ParseGCAssembly(**assm, entry);
            }
        }
    }
}


void CGC_Assembly_Parser::x_ParseGCSequence(const CGC_Sequence& gc_seq,
                                            const CGC_Sequence* parent_seq,
                                            CRef<CSeq_entry>    parent_entry,
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
    TSeqIds synonyms;
    synonyms.insert(CSeq_id_Handle::GetHandle(*id));
    if ( gc_seq.IsSetSeq_id_synonyms() ) {
        ITERATE(CGC_Sequence::TSeq_id_synonyms, it, gc_seq.GetSeq_id_synonyms()) {
            // Add conversion for each synonym which can be used
            // as a source id.
            const CGC_TypedSeqId& it_id = **it;
            switch ( it_id.Which() ) {
            case CGC_TypedSeqId::e_Genbank:
                synonyms.insert(CSeq_id_Handle::GetHandle(it_id.GetGenbank().GetPublic()));
                if ( it_id.GetGenbank().IsSetGi() ) {
                    synonyms.insert(CSeq_id_Handle::GetHandle(it_id.GetGenbank().GetGi()));
                }
                if ( it_id.GetGenbank().IsSetGpipe() ) {
                    synonyms.insert(CSeq_id_Handle::GetHandle(it_id.GetGenbank().GetGpipe()));
                }
                break;
            case CGC_TypedSeqId::e_Refseq:
            {
                // If some of the ids is used in the structure (see above),
                // ignore all refseq ids.
                synonyms.insert(CSeq_id_Handle::GetHandle(it_id.GetRefseq().GetPublic()));
                if ( it_id.GetRefseq().IsSetGi() ) {
                    synonyms.insert(CSeq_id_Handle::GetHandle(it_id.GetRefseq().GetGi()));
                }
                if (it_id.GetRefseq().IsSetGpipe()) {
                    synonyms.insert(CSeq_id_Handle::GetHandle(it_id.GetRefseq().GetGpipe()));
                }
                break;
            }
            case CGC_TypedSeqId::e_Private:
                // Ignore private local ids.
                if ((m_Flags & fIgnoreLocalIds) == 0  ||
                    !it_id.GetPrivate().IsLocal()) {
                    synonyms.insert(CSeq_id_Handle::GetHandle(it_id.GetPrivate()));
                }
                break;
            case CGC_TypedSeqId::e_External:
                if ((m_Flags & fIgnoreExternalIds) == 0) {
                    synonyms.insert(CSeq_id_Handle::GetHandle(it_id.GetExternal().GetId()));
                }
                break;
            default:
                NCBI_THROW(CAssemblyParserException, eUnsupported,
                           "Unsupported alias type in GC-Sequence synonyms");
                break;
            }
        }
        // The sequence is referencing itself?
        if (synonyms.find(struct_syn) != synonyms.end()) {
            x_ParseGCSequence(
                *gc_seq.GetSequences().front()->GetSeqs().front(),
                parent_seq,
                parent_entry,
                id);
            return;
        }
    }

    CRef<CSeq_entry> entry;
    if ( gc_seq.IsSetSequences() ) {
        entry.Reset(new CSeq_entry);
        x_InitSeq_entry(entry, parent_entry);
    }
    else {
        entry = parent_entry;
    }

    if (gc_seq.IsSetDescr()  &&  (m_Flags & fIgnoreDescr) == 0) {
        const CSeq_descr& descr = gc_seq.GetDescr();
        ITERATE(CSeq_descr::Tdata, desc, descr.Get()) {
            CRef<CSeqdesc> desc_copy(new CSeqdesc);
            desc_copy->Assign(**desc);
            entry->SetDescr().Set().push_back(desc_copy);
        }
    }
    if (gc_seq.IsSetAnnot()  &&  (m_Flags & fIgnoreAnnots) == 0) {
        ITERATE(CGC_Sequence::TAnnot, annot, gc_seq.GetAnnot()) {
            CRef<CSeq_annot> annot_copy(new CSeq_annot);
            annot_copy->Assign(**annot);
            entry->SetAnnot().push_back(annot_copy);
        }
    }

    // Create virtual bioseq and use it to initialize the mapper
    if ( gc_seq.IsSetStructure() ) {
        x_AddBioseq(entry, synonyms, &gc_seq.GetStructure());
    }
    else {
        // Create literal sequence
        x_AddBioseq(entry, synonyms, NULL);
    }
    if ( !parent_seq ) {
        // Save top-level sequences.
        m_TopSeqs.insert(CSeq_id_Handle::GetHandle(*id));
    }

    if ( gc_seq.IsSetSequences() ) {
        CRef<CSeq_entry> sub_entry(new CSeq_entry);
        x_InitSeq_entry(sub_entry, entry);
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
                x_ParseGCSequence(**tseq, parent, sub_entry, null);
            }
        }
    }
}


void CGC_Assembly_Parser::x_AddBioseq(CRef<CSeq_entry>  parent_entry,
                                      const TSeqIds&    synonyms,
                                      const CDelta_ext* delta)
{
    CRef<CBioseq> bioseq(new CBioseq);
    ITERATE(TSeqIds, syn, synonyms) {
        // Do not add bioseqs with duplicate ids.
        if ((m_Flags & fSkipDuplicates) != 0  &&
            !m_AllSeqs.insert(*syn).second ) {
            return;
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
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSeq(*bioseq);
    parent_entry->SetSet().SetSeq_set().push_back(entry);
}


END_SCOPE(objects)
END_NCBI_SCOPE
