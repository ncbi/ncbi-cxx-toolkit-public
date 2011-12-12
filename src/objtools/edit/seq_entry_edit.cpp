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
* Author: Mati Shomrat, NCBI
*
* File Description:
*   High level Seq-entry edit, for meaningful combination of Seq-entries.
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/bioseq_set_handle.hpp>
#include <objtools/edit/edit_exception.hpp>
#include <objtools/edit/seq_entry_edit.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)


static void s_AddBioseqToPartsSet
(CBioseq_set_EditHandle& parts,
 CBioseq_EditHandle& seq)
{
    _ASSERT(parts  &&  seq);
    _ASSERT(parts.GetClass() == CBioseq_set::eClass_parts);

    CSeq_inst::TMol seq_mol   = seq.GetInst_Mol();
    // Test that the new part has the same molecular type as other parts
    for ( CSeq_entry_CI it(parts); it; ++it ) {
        if ( it->IsSeq()  &&  it->GetSeq().GetInst_Mol() != seq_mol ) {
            NCBI_THROW(CEditException, eInvalid, 
                "Unable to add part due to conflicting molecular types");
        }
    }

    parts.TakeBioseq(seq);
}


static void s_AddPartToSegmentedBioseq
(const CBioseq_EditHandle& seg,
 const CBioseq_EditHandle& part)
{
    _ASSERT(seg  &&  part);
    _ASSERT(seg.GetInst_Repr() == CSeq_inst::eRepr_seg);

    // add a new reference to part in the segmented bioseq
    // NB: temporary implementation, should be done through CSeqMap when
    // this feature become available.
    CRef<CSeq_id> id(new CSeq_id);
    id->Assign(*part.GetSeqId());
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetWhole(*id);
    // create a new CSeq_ext object
    CRef<CSeq_ext> ext(new CSeq_ext);
    // copy content of exisiting CSeq_ext object into new one.
    CSeg_ext::Tdata& segs = ext->SetSeg().Set();
    if ( seg.CanGetInst_Ext() ) {
        copy(seg.GetInst_Ext().GetSeg().Get().begin(),
             seg.GetInst_Ext().GetSeg().Get().end(),
             back_inserter(segs));
    }
    // add reference to the new part
    segs.push_back(loc);
    // set the new one as the ext object
    seg.SetInst_Ext(*ext);
}


static void s_AddBioseqToSegset
(CBioseq_set_EditHandle& segset,
 CBioseq_EditHandle& part)
{
    CBioseq_set_EditHandle parts;
    CBioseq_EditHandle master;
    for ( CSeq_entry_I it(segset); it; ++it ) {
        if ( it->IsSet()  &&
             it->GetSet().GetClass() == CBioseq_set::eClass_parts ) {
            parts = it->SetSet();
        } else if ( it->IsSeq()  &&
            it->GetSeq().GetInst_Repr() == CSeq_inst::eRepr_seg ) {
            master = it->SetSeq();
        }
    }

    if ( !parts  ||  !master ) {
        NCBI_THROW(CEditException, eInvalid, "Missing a component from segset");
    }

    // add part to parts set
    s_AddBioseqToPartsSet(parts, part);
    s_AddPartToSegmentedBioseq(master, part);
}


//  --  AddSeqEntryToSeqEntry

void AddSeqEntryToSeqEntry
(const CSeq_entry_Handle& target,
 const CSeq_entry_Handle& insert)
{
    if ( !target  ||  !insert ) {
        return;
    }

    if ( target.IsSeq()  &&  insert.IsSeq() ) {
        AddBioseqToBioseq(target.GetSeq(), insert.GetSeq());
    } else if ( target.IsSet()  &&  insert.IsSeq() ) {
        AddBioseqToBioseqSet(target.GetSet(), insert.GetSeq());
    }
}


//  --  AddBioseqToBioseq

// Create a nuc-prot set containing the two bioseqs
static void s_AddProtToNuc(const CBioseq_EditHandle& nuc, const CBioseq_EditHandle& prot)
{
    _ASSERT(CSeq_inst::IsNa(nuc.GetInst_Mol()));
    _ASSERT(CSeq_inst::IsAa(prot.GetInst_Mol()));

    CSeq_entry_EditHandle nuc_entry = nuc.GetParentEntry();
    CSeq_entry_EditHandle::TSet nuc_prot = 
        nuc_entry.ConvertSeqToSet(CBioseq_set::eClass_nuc_prot);
    prot.MoveTo(nuc_prot);
}


static CSeq_id* s_MakeUniqueLocalId(void)
{
    static size_t count = 0;

    return new CSeq_id("lcl|segset_" + NStr::NumericToString(++count));
}


// The two bioseqs are parts of a segset
static void s_AddBioseqToBioseq(const CBioseq_EditHandle& to, const CBioseq_EditHandle& add)
{
    _ASSERT(to.GetInst_Mol() == add.GetInst_Mol());

    CSeq_entry_EditHandle segset = to.GetParentEntry();
    segset.ConvertSeqToSet(CBioseq_set::eClass_segset);
    _ASSERT(segset.IsSet());

    CSeq_entry_EditHandle parts = to.GetParentEntry();
    parts.ConvertSeqToSet(CBioseq_set::eClass_parts);
    parts.TakeBioseq(add);

    // the segmented bioseq
    // NB: Code for creating the segmented bioseq should change
    // when the functionality is provided by object manager
    CRef<CBioseq> seq(new CBioseq);
    _ASSERT(seq);
    CRef<CSeq_id> id(s_MakeUniqueLocalId());
    seq->SetId().push_back(id);
    CBioseq_EditHandle master = segset.AttachBioseq(*seq, 0);
    
    master.SetInst_Repr(CSeq_inst::eRepr_seg);
    master.SetInst_Mol(to.GetInst_Mol());
    master.SetInst_Length(to.GetInst_Length() + add.GetInst_Length());

    s_AddPartToSegmentedBioseq(master, to);
    s_AddPartToSegmentedBioseq(master, add);
}


void AddBioseqToBioseq
(const CBioseq_Handle& to,
 const CBioseq_Handle& add)
{
    if ( !to ||  !add ) {
        return;
    }

    CBioseq_Handle::TInst_Mol to_mol  = to.GetInst_Mol();
    CBioseq_Handle::TInst_Mol add_mol = add.GetInst_Mol();
    
    // adding a protein to a nucletide
    if ( CSeq_inst::IsNa(to_mol)  &&  CSeq_inst::IsAa(add_mol) ) {
        s_AddProtToNuc(to.GetEditHandle(), add.GetEditHandle());
    } else if ( to_mol == add_mol ) {
        // these are two parts of a segset
        s_AddBioseqToBioseq(to.GetEditHandle(), add.GetEditHandle());
    }
}


//  --  AddBioseqToBioseqSet

// A nuc-prot associates one or more proteins with a single 
// nucleotide.

static void s_AddBioseqToNucProtSet
(CBioseq_set_EditHandle& nuc_prot,
 CBioseq_EditHandle& seq)
{
    _ASSERT(nuc_prot  &&  seq);
    _ASSERT(nuc_prot.GetClass() == CBioseq_set::eClass_nuc_prot);


    if ( CSeq_inst::IsAa(seq.GetInst_Mol()) ) {
        // if the new bioseq is a protein simply add it
        seq.MoveTo(nuc_prot);
    } else {
        CSeq_entry_CI it(nuc_prot);
        while ( it ) {
            if ( it->IsSeq()  &&  
                 CSeq_inst::IsNa(it->GetSeq().GetInst_Mol()) ) {
                break;
            }
            ++it;
        }
        if ( it ) {
            AddBioseqToBioseq(it->GetSeq(), seq);
        } else {
            seq.MoveTo(nuc_prot, 0); // add the nucleotide as the first entry
        }
    }
}


void AddBioseqToBioseqSet(const CBioseq_set_Handle& set, const CBioseq_Handle& seq)
{
    if ( !set  ||  !seq ) {
        return;
    }

    CBioseq_EditHandle     seq_edit = seq.GetEditHandle();
    CBioseq_set_EditHandle set_edit = set.GetEditHandle();
    if ( !seq_edit  ||  !set_edit ) {
        return;
    }

    switch ( set_edit.GetClass() ) {
    case CBioseq_set::eClass_nuc_prot:
        s_AddBioseqToNucProtSet(set_edit, seq_edit);
        break;
    case CBioseq_set::eClass_segset:
        s_AddBioseqToSegset(set_edit, seq_edit);
        break;
    case CBioseq_set::eClass_conset:
        break;
    case CBioseq_set::eClass_parts:
        s_AddBioseqToPartsSet(set_edit, seq_edit);
        break;
    case CBioseq_set::eClass_gibb:
        break;
    case CBioseq_set::eClass_gi:
        break;
    case CBioseq_set::eClass_pir:
        break;
    case CBioseq_set::eClass_pub_set:
        break;
    case CBioseq_set::eClass_equiv:
        break;
    case CBioseq_set::eClass_swissprot:
        break;
    case CBioseq_set::eClass_pdb_entry:
        break;
    case CBioseq_set::eClass_gen_prod_set:
        break;
    case CBioseq_set::eClass_other:
        break;

    case CBioseq_set::eClass_genbank:
    case CBioseq_set::eClass_mut_set:
    case CBioseq_set::eClass_pop_set:
    case CBioseq_set::eClass_phy_set:
    case CBioseq_set::eClass_eco_set:
    case CBioseq_set::eClass_wgs_set:
    default:
        // just move the bioseq to the set
        seq_edit.MoveTo(set_edit);
        break;
    }
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
