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
* Author: Mati Shomrat, Jie Chen, NCBI
*
* File Description:
*   High level Seq-entry edit, for meaningful combination of Seq-entries.
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/general/User_object.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/bioseq_set_handle.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/seq_descr_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include <objtools/edit/edit_exception.hpp>
#include <objtools/edit/seq_entry_edit.hpp>
#include <objtools/edit/loc_edit.hpp>
#include <set>
#include <sstream>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

namespace {
    // This works just like a map except that when you iterate over it,
    // it goes by order of initial insertion.
    // We use private inheritance because map's destructor is non-virtual
    // Maybe this can be put in a shared place and others can use it,
    // but it would need more cleanup first.
    template<typename Key, typename Data, typename Compare = less<Key>, typename Alloc = allocator<pair<const Key,Data> > >
    class CMapWithOriginalOrderingIteration : private map<Key, Data, Compare, Alloc>  
    {
    public:
        typedef typename map<Key, Data, Compare, Alloc>::value_type value_type;
        typedef typename map<Key, Data, Compare, Alloc>::iterator iterator;
        typedef typename map<Key, Data, Compare, Alloc>::const_iterator const_iterator;
        // "begin" deliberately omitted
        using map<Key, Data, Compare, Alloc>::end;
        using map<Key, Data, Compare, Alloc>::empty;
        using map<Key, Data, Compare, Alloc>::size;
        using map<Key, Data, Compare, Alloc>::find;
        // add more "using" statements if you find more things from map<> that
        // you want to use.  Make sure to keep consistency, though.
        // For example, don't just use "using" on "erase".  Implement
        // erase to be a wrapper over map's erase and make sure to
        // update m_keysInOriginalOrder, etc.

        // we override insert so we can keep track of ordering
        // of keys
        pair<iterator, bool> insert(const value_type& x)
        {
            pair<iterator, bool> result = map<Key, Data, Compare, Alloc>::insert(x);
            if( result.second ) {
                m_keysInOriginalOrder.push_back( x.first );
            }
            return result;
        }

        Data & 
            operator[](const Key & k)
        {
            iterator find_iter = find(k);
            if( find_iter != end() ) {
                // already in map, just return it
                return find_iter->second;
            }

            // not in map, so add
            pair<iterator, bool> result = insert( 
                value_type(k, Data()) );
            _ASSERT( result.second );
            return result.first->second;
        }

        typedef vector<Key> TKeyVec;
        const TKeyVec & GetKeysInOriginalOrder(void) const {
            _ASSERT( m_keysInOriginalOrder.size() == size() );
            return m_keysInOriginalOrder;
        }

    private:
        TKeyVec m_keysInOriginalOrder;
    };

    static char descr_insert_order [] = {
        CSeqdesc::e_Title,
        CSeqdesc::e_Source,
        CSeqdesc::e_Molinfo,
        CSeqdesc::e_Het,
        CSeqdesc::e_Pub,
        CSeqdesc::e_Comment,
        CSeqdesc::e_Name,
        CSeqdesc::e_User,
        CSeqdesc::e_Maploc,
        CSeqdesc::e_Region,
        CSeqdesc::e_Num,
        CSeqdesc::e_Dbxref,
        CSeqdesc::e_Mol_type,
        CSeqdesc::e_Modif,
        CSeqdesc::e_Method,
        CSeqdesc::e_Org,
        CSeqdesc::e_Sp,
        CSeqdesc::e_Pir,
        CSeqdesc::e_Prf,
        CSeqdesc::e_Pdb,
        CSeqdesc::e_Embl,
        CSeqdesc::e_Genbank,
        CSeqdesc::e_Modelev,
        CSeqdesc::e_Create_date,
        CSeqdesc::e_Update_date,
        0
    };

    // inverted matrix to speed up the seqdesc sorting
    class CSeqdescSortMap: public vector<char>
    {
    public:
        void Init()
        {            
            resize(sizeof(descr_insert_order)/sizeof(char), kMax_Char);
            char index = 0;
            while (descr_insert_order[index] != 0)
            {
                if (descr_insert_order[index] >= size())
                    resize(descr_insert_order[index], kMax_Char);
                at(descr_insert_order[index]) = index;
                ++index;
            }
        }
    };
    static CSeqdescSortMap seqdesc_sortmap;

    struct CompareSeqdesc
    {
        CompareSeqdesc()
        {
            if (seqdesc_sortmap.empty())
                seqdesc_sortmap.Init();
        }

        static char mapit(CSeqdesc::E_Choice c)
        {
            if (c<0 || c>=seqdesc_sortmap.size())
                return kMax_Char;
            return seqdesc_sortmap[c];
        }

        bool operator()(const CRef<CSeqdesc>& l, const CRef<CSeqdesc>& r) const
        {
            return mapit(l->Which()) < mapit(r->Which());
        }
    };
}

CConstRef <CDelta_seq> GetDeltaSeqForPosition(const unsigned pos, const CBioseq_Handle seq_hl, CScope* scope, unsigned& left_endpoint)
{
   if (!seq_hl || !seq_hl.IsNa() || !seq_hl.IsSetInst_Repr()
             || seq_hl.GetInst_Repr() != CSeq_inst :: eRepr_delta
             || !seq_hl.GetInst().CanGetExt()
             || !seq_hl.GetInst().GetExt().IsDelta()) {
      return CConstRef <CDelta_seq>(NULL);
   }

   size_t offset = 0;

   int len = 0;
   ITERATE (list <CRef <CDelta_seq> >, it, seq_hl.GetInst_Ext().GetDelta().Get()) {
        if ((*it)->IsLiteral()) {
            len = (*it)->GetLiteral().GetLength();
        } else if ((*it)->IsLoc()) {
            len = sequence::GetLength((*it)->GetLoc(), scope);
        }
        if (pos >= offset && pos < offset + len) {
            left_endpoint = offset;
            return (*it);
        } else {
            offset += len;
        }
   }
   return (CConstRef <CDelta_seq>(NULL));
};

bool IsDeltaSeqGap(CConstRef <CDelta_seq> delta)
{
  if (delta->IsLoc()) return false;
  if (delta->GetLiteral().CanGetSeq_data() && delta->GetLiteral().GetSeq_data().IsGap()){
      return true;
  }
  else return false;
};

bool Does5primerAbutGap(const CSeq_feat& feat, CBioseq_Handle seq_hl)
{
  if (!seq_hl) return false;

  unsigned start = feat.GetLocation().GetTotalRange().GetFrom();  // Positional
  if (!start) return false;

  CSeqVector seq_vec(seq_hl, CBioseq_Handle::eCoding_Iupac, eNa_strand_plus);
  unsigned i=0;
  for (CSeqVector_CI it = seq_vec.begin(); it; ++ it, i++) {
     if (i < (start - 1) ) continue;
     if (it.IsInGap()) return true;
  }
  return false;
};

bool Does3primerAbutGap(const CSeq_feat& feat, CBioseq_Handle seq_hl)
{
  if (!seq_hl) return false;

  unsigned stop = feat.GetLocation().GetTotalRange().GetTo(); // positional

  CSeqVector seq_vec(seq_hl, CBioseq_Handle::eCoding_Iupac, eNa_strand_plus);
  if (stop >= seq_vec.size() - 1) return false;
  unsigned i=0;
  for (CSeqVector_CI it = seq_vec.begin(); it; ++ it, i++) {
     if (i < (stop + 1) ) continue;
     if (it.IsInGap()) return true;
  }
  return false;
};

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


bool IsSeqDescInList(const CSeqdesc& desc, const CSeq_descr& set)
{
    ITERATE(CSeq_descr::Tdata, it, set.Get()) {
        if ((*it)->Equals(desc)) {
            return true;
        } else if ((*it)->IsPub() &&
                   desc.IsPub() &&
                   (*it)->GetPub().GetPub().SameCitation(desc.GetPub().GetPub())) {
            return true;
        }
    }
    return false;
}


void AddSeqdescToSeqDescr(const CSeqdesc& desc, CSeq_descr& seq_descr)
{
    CRef<CSeqdesc> d(new CSeqdesc());
    d->Assign(desc);
    seq_descr.Set().push_back(d);
}


void AddSeqdescToBioseq(const CSeqdesc& desc, CBioseq& seq)
{
    if (!seq.IsSetDescr() || !IsSeqDescInList(desc, seq.GetDescr())) {
        AddSeqdescToSeqDescr(desc, seq.SetDescr());
    }
}


void AddSeqdescToBioseqSet(const CSeqdesc& desc, CBioseq_set& set)
{
    if (!set.IsSetDescr() || !IsSeqDescInList(desc, set.GetDescr())) {
        AddSeqdescToSeqDescr(desc, set.SetDescr());
    }
}


bool AddSeqdescToSeqEntryRecursively(CSeq_entry& entry, CSeqdesc& desc)
{
    bool rval = false;
    if (entry.IsSeq()) {
        AddSeqdescToBioseq(desc, entry.SetSeq());
        rval = true;
    } else if (entry.IsSet()) {
        if (entry.GetSet().IsSetClass() && 
            (entry.GetSet().GetClass() == CBioseq_set::eClass_nuc_prot ||
             entry.GetSet().GetClass() == CBioseq_set::eClass_segset)) {
            AddSeqdescToBioseqSet(desc, entry.SetSet());
            rval = true;
        } else if (entry.GetSet().IsSetSeq_set()) {
            NON_CONST_ITERATE(CBioseq_set::TSeq_set, it, entry.SetSet().SetSeq_set()) {
                rval |= AddSeqdescToSeqEntryRecursively(**it, desc);
            }
            if (!rval) {
                AddSeqdescToBioseqSet(desc, entry.SetSet());
                rval = true;
            }
        }
    }
    return rval;
}


CRef<CSeq_entry> SeqEntryFromSeqSubmit(const CSeq_submit& submit)
{
    CRef<CSeq_entry> entry(new CSeq_entry());
    if (!submit.IsEntrys() || submit.GetData().GetEntrys().size() == 0) {
        return CRef<CSeq_entry>(NULL);
    }

    // Copy Seq-entry data from Seq-submit to new Seq-entry
    if (submit.GetData().GetEntrys().size() > 1) {
        entry->SetSet().SetClass(CBioseq_set::eClass_genbank);
        ITERATE(CSeq_submit::TData::TEntrys, it, submit.GetData().GetEntrys()) {
            CRef<CSeq_entry> e(new CSeq_entry());
            e->Assign(**it);
            entry->SetSet().SetSeq_set().push_back(e);
        }
    } else {
        entry->Assign(*(submit.GetData().GetEntrys().front()));
    }

    // Create cit-sub pub for Seq-entry
    if (submit.IsSetSub() && submit.GetSub().IsSetCit()) {
        CRef<CPub> pub(new CPub());
        pub->SetSub().Assign(submit.GetSub().GetCit());
        CRef<CSeqdesc> pdesc(new CSeqdesc());
        pdesc->SetPub().SetPub().Set().push_back(pub);
        if (entry->IsSeq()) {
            AddSeqdescToBioseq(*pdesc, entry->SetSeq());
        } else {
            AddSeqdescToSeqEntryRecursively(*entry, *pdesc);
        }
    }
    return entry;
}


static bool s_IsSingletonSet(
    const CBioseq_set_Handle & bioseq_set )
{
    CSeq_entry_CI direct_child_ci(bioseq_set, CSeq_entry_CI::eNonRecursive );
    if( ! direct_child_ci ) {
        // not singleton: has no children
        return false;
    }
    ++direct_child_ci;
    if( direct_child_ci ) {
        // not singleton: has more than one child
        return false;
    }

    // not singleton if has any alignment annots
    CSeq_annot_CI annot_ci( bioseq_set, CSeq_annot_CI::eSearch_entry );
    for( ; annot_ci; ++annot_ci ) {
        if( annot_ci->IsAlign() ) {
            return false;
        }
    }

    // it's a singleton: it passed all tests
    return true;
}

static void s_PromoteSingletonSetsInSet(
    const CBioseq_set_Handle & bioseq_set_h )
{
    typedef vector<CSeq_entry_EditHandle> TBioseqSetsToPromote;
    TBioseqSetsToPromote bioseqSetsToPromote;

    CSeq_entry_CI direct_child_set_ci( bioseq_set_h, 
        CSeq_entry_CI::eNonRecursive, CSeq_entry::e_Set );
    for( ; direct_child_set_ci; ++direct_child_set_ci ) {

        CBioseq_set_EditHandle direct_child_set_eh = 
            direct_child_set_ci->GetSet().GetEditHandle();
        if( s_IsSingletonSet(direct_child_set_eh) ) {

            // get handle to the sets one child
            CSeq_entry_CI direct_child_direct_child_ci( 
                direct_child_set_eh, CSeq_entry_CI::eNonRecursive );
            CSeq_entry_EditHandle direct_child_direct_child_eh = 
                direct_child_direct_child_ci->GetEditHandle();
            ++direct_child_direct_child_ci;
            _ASSERT( ! direct_child_direct_child_ci );

            // remove titles, and then other descriptor
            // types will be moved to the children of this child
            CSeqdesc_CI::TDescChoices desc_choices_to_erase;
            desc_choices_to_erase.push_back( CSeqdesc::e_Title );
            BioseqSetDescriptorPropagateDown( direct_child_set_eh, 
                desc_choices_to_erase );

            // push down annotation
            direct_child_direct_child_eh.TakeAllAnnots( 
                direct_child_set_eh.GetParentEntry() );

            // remember for later because removing now will 
            // confuse the iteration
            bioseqSetsToPromote.push_back( 
                direct_child_direct_child_eh );
        }
    }

    // perform promotions requested
    ITERATE( TBioseqSetsToPromote, promoted_set_it, bioseqSetsToPromote ) {
        _ASSERT( bioseq_set_h.GetSeq_entry_Index( *promoted_set_it ) >= 0 );
        promoted_set_it->GetParentEntry().Remove();
        bioseq_set_h.GetEditHandle().TakeEntry( *promoted_set_it );
    }
}

// Compares CSerialObjects by their ASN.1 output (with caching).
// In the future, it would be better to have something more efficient
// than conversion to text ASN.1  (E.g. if CSerialObject gets a
// comparison function similar to the Equals function it already has)
template <class T>
struct SSerialObjectLessThan {
public:
    bool operator()( const CConstRef<T> & lhs,
        const CConstRef<T> & rhs ) const
    {
        // NULL first
        if( lhs.IsNull() ) {
            if( rhs.IsNull() ) {
                return false; // equal
            } else {
                // lhs is first
                return true;
            }
        } else if ( rhs.IsNull() ) {
            // rhs is first
            return false;
        }

        _ASSERT( lhs && rhs );

        const string & lhs_asn = x_GetAsnText( lhs );
        const string & rhs_asn = x_GetAsnText( rhs );

        return lhs_asn < rhs_asn;
    }

private:
    typedef map< CConstRef<T>, string > TMapObjToTextAsn;
    mutable TMapObjToTextAsn m_ObjAsnCache;

    // retrieves from cache, if possible
    const string & x_GetAsnText( const CConstRef<T> & obj ) const
    {
        string & asn_text = m_ObjAsnCache[obj];
        if( asn_text.empty() ) {
            // not in cache, so add to cache
            stringstream asn_strm;
            asn_strm << MSerial_AsnText << *obj;
            asn_strm.str().swap( asn_text );
        }

        return asn_text;
    }
};


template<class T>
struct CSerialObjectSet {
    // we just have to delcare a class because
    // we can't have template-parameterized typedefs
    typedef set< CConstRef<T>, SSerialObjectLessThan<T> > Type;    
};

static void s_MakeGroupsForUniqueValues( 
    const CSeq_entry_Handle & target, 
    const CScope::TBioseqHandles & bioseq_handles )
{
    if( ! target || ! target.IsSet() ) {
        // can't split something that's not a bioseq-set
        return;
    }

    CBioseq_set_Handle bioseq_set_h = target.GetSet();

    CBioseq_set_Handle target_parent_h  = target.GetParentBioseq_set();

    CBioseq_set::EClass child_class = ( 
        bioseq_set_h.IsSetClass() ?
        bioseq_set_h.GetClass() :
        CBioseq_set::eClass_not_set );

    bool child_became_parent = false;
    if( ! target_parent_h ) {
        // this set has no parent, so make it the parent set, class GenBank,
        // and create two new sets using the original set class as members of this set
        target_parent_h = bioseq_set_h;
        child_became_parent = true;
    }

    // create the bioseq-set that will hold the new ones
    CBioseq_set_Handle new_bioseq_set;
    {
        CRef<CSeq_entry> pEntry( new CSeq_entry );
        pEntry->SetSet().SetClass( child_class );
        new_bioseq_set = target_parent_h.GetEditHandle().AttachEntry( *pEntry, 0 ).GetSet();
    }

    // as we go along, accumulate the Seq-descrs and annots that we would like to add to 
    // new_bioseq_set.
    typedef vector< CConstRef<CSeqdesc> > TDescRefVec;
    TDescRefVec vecDescsToAddToNewBioseqSet;

    typedef vector< CConstRef<CSeq_annot> > TAnnotRefVec;
    TAnnotRefVec vecAnnotsToAddToNewBioseqSet;

    // add SeqEntries for this category here
    // AddItemListToSet(pBioseqSet, bioseq_handles, TRUE /* for_segregate */ );
    ITERATE( CScope::TBioseqHandles, bioseq_it, bioseq_handles ) {
        
        // If it's directly in a nuc-prot bioseq-set return its seq-entry 
        // because nuc-prot sets need to travel together,
        // otherwise return the seq-entry that directly contains this bioseq
        // This is like C toolkit's GetBestTopParentForData
        CSeq_entry_Handle best_entry_for_bioseq = bioseq_it->GetParentEntry();
        if( best_entry_for_bioseq && 
            best_entry_for_bioseq.HasParentEntry() ) 
        {
            CSeq_entry_Handle parent_entry = best_entry_for_bioseq.GetParentEntry();
            if( parent_entry && parent_entry.IsSet() &&
                FIELD_EQUALS( parent_entry.GetSet(), 
                    Class, CBioseq_set::eClass_nuc_prot ) ) 
            {
                best_entry_for_bioseq = parent_entry;
            }
        }
        if( ! best_entry_for_bioseq ) {
            continue;
        }

        // 
        CBioseq_set_Handle orig_parent = best_entry_for_bioseq.GetParentBioseq_set();

        if( orig_parent ) {

            // if new_bioseq_set was of class genbank set, 
            // it can get the orig_parent's class instead
            if( orig_parent.IsSetClass() && 
                ( ! new_bioseq_set.IsSetClass() || 
                  new_bioseq_set.GetClass() == CBioseq_set::eClass_genbank ) )
            {
                new_bioseq_set.GetEditHandle().SetClass( orig_parent.GetClass() );
            }

            // remember the descriptors and annots that we want to add later

            CSeqdesc_CI desc_ci( orig_parent.GetParentEntry(), CSeqdesc::e_not_set, 1 );
            for( ; desc_ci; ++desc_ci ) {
                vecDescsToAddToNewBioseqSet.push_back( 
                    CConstRef<CSeqdesc>(&*desc_ci) );
            }

            CSeq_annot_CI annot_ci( orig_parent, CSeq_annot_CI::eSearch_entry );
            for( ; annot_ci; ++annot_ci ) {
                vecAnnotsToAddToNewBioseqSet.push_back( annot_ci->GetCompleteSeq_annot() );
            }
        }

        // remove from orig_parent (if any) and add to the new_bioseq_set
        new_bioseq_set.GetEditHandle().TakeEntry( best_entry_for_bioseq.GetEditHandle() );
    }

    // add unique descriptors
    {
        CSerialObjectSet<CSeqdesc>::Type descsSeen;
        // add the descriptors already in the destination
        CSeqdesc_CI dest_desc_ci( new_bioseq_set.GetParentEntry(), CSeqdesc::e_not_set, 1 );
        for( ; dest_desc_ci; ++dest_desc_ci ) {
            descsSeen.insert( CConstRef<CSeqdesc>(&*dest_desc_ci) );
        }

        ITERATE( TDescRefVec, src_seqdesc_ref_it, vecDescsToAddToNewBioseqSet ) {
            if( descsSeen.find(*src_seqdesc_ref_it) != descsSeen.end() ) {
                // skip because it duplicates an earlier one
                continue;
            }

            CRef<CSeqdesc> pNewDesc( SerialClone(**src_seqdesc_ref_it) );
            new_bioseq_set.GetEditHandle().AddSeqdesc( *pNewDesc );
            descsSeen.insert( *src_seqdesc_ref_it );
        }
    }

    // add unique annotations
    {
        CSerialObjectSet<CSeq_annot>::Type annotsSeen;
        // add the annots already in the destination
        CSeq_annot_CI dest_annot_ci( new_bioseq_set, CSeq_annot_CI::eSearch_entry );
        for( ; dest_annot_ci; ++dest_annot_ci ) {
            annotsSeen.insert( dest_annot_ci->GetCompleteSeq_annot() );
        }

        ITERATE( TAnnotRefVec, src_annot_it, vecAnnotsToAddToNewBioseqSet ) {
            if( annotsSeen.find(*src_annot_it) != annotsSeen.end() ) {
                // skip because it duplicates an earlier one
                continue;
            }

            CRef<CSeq_annot> pNewAnnot( SerialClone(**src_annot_it) );
            new_bioseq_set.GetEditHandle().AttachAnnot( *pNewAnnot );
            annotsSeen.insert( *src_annot_it );
        }
    }


    if( child_became_parent ) {
        // get siblings of new_bioseq_set and see if any need to be wrapped into a set

        // fill in list of siblings
        vector<CSeq_entry_Handle> siblingVec;
        {
            CSeq_entry_CI new_set_sibling_ci( target_parent_h, CSeq_entry_CI::eNonRecursive );
            // skip the first one, which should equal new_bioseq_set
            _ASSERT( *new_set_sibling_ci == new_bioseq_set.GetParentEntry() );
            ++new_set_sibling_ci;

            for( ; new_set_sibling_ci; ++new_set_sibling_ci ) {
                siblingVec.push_back(*new_set_sibling_ci);
            }
        }

        bool bNeedsNewSet = false;
        ITERATE( vector<CSeq_entry_Handle>, sibling_it, siblingVec ) {
            if( sibling_it->IsSeq() ) {
                bNeedsNewSet = true;
                break;
            }

            if( sibling_it->IsSet() && 
                FIELD_EQUALS( sibling_it->GetSet(), Class, 
                    CBioseq_set::eClass_nuc_prot ) )
            {
                bNeedsNewSet = true;
                break;
            }
        }
        if( bNeedsNewSet ) {
            // remaining entries must be put into another set
            CRef<CSeq_entry> pRemainderEntry( new CSeq_entry );
            pRemainderEntry->SetSet().SetClass( child_class );
            CBioseq_set_Handle remainder_bioseq_set = 
                target_parent_h.GetEditHandle().AttachEntry( *pRemainderEntry ).GetSet();

            ITERATE( vector<CSeq_entry_Handle>, sibling_it, siblingVec ) {
                if( *sibling_it == remainder_bioseq_set.GetParentEntry() ) {
                    continue;
                }

                remainder_bioseq_set.GetEditHandle().TakeEntry( sibling_it->GetEditHandle() );
            }

            // take descriptors from parent onto our new remainder set
            // (we do NOT have to check for uniqueness because remainder_bioseq_set
            // starts off with no Seq-descrs)
            CSeq_descr_CI parent_descr_ci(target_parent_h, 1);
            for( ; parent_descr_ci; ++parent_descr_ci ) {
                CRef<CSeq_descr> pRemainderSeqDescr( SerialClone(*parent_descr_ci) );
                remainder_bioseq_set.GetEditHandle().AddSeq_descr( *pRemainderSeqDescr );
            }
            // set to genbank set
            target_parent_h.GetEditHandle().SetClass(  CBioseq_set::eClass_genbank );
            // remove all Seq_descrs
            target_parent_h.GetEditHandle().ResetDescr();
        }
    }

    s_PromoteSingletonSetsInSet( target_parent_h );
}

void SegregateSetsByBioseqList(
    const CSeq_entry_Handle & target,
    const CScope::TBioseqHandles & bioseq_handles )
{
    if( ! target || ! target.IsSet() ) {
        // can't split something that's not a bioseq-set
        return;
    }

    CBioseq_set_Handle bioseq_set_h = target.GetSet();


    // MakeGroupsForUniqueValues
    s_MakeGroupsForUniqueValues( target, 
        bioseq_handles );

    // copy bioseq list alignments
    TVecOfSeqEntryHandles vecOfSeqEntryHandles;
    CSeq_entry_CI direct_child_ci(bioseq_set_h, CSeq_entry_CI::eNonRecursive);
    for( ; direct_child_ci; ++direct_child_ci ) {
        vecOfSeqEntryHandles.push_back(*direct_child_ci);
    }

    // For every direct align under each direct child, figure out where it belongs:
    //     See if it belongs to one of the other children or more than one or none
    //     (for none, it should go to *all* of them, and for more than one, it's destroyed )
    DivvyUpAlignments(vecOfSeqEntryHandles);
}

// typedefs used by DivvyUpAlignments and its helper functions
typedef map<CSeq_entry_Handle, CSeq_entry_Handle> TMapDescendentToInputEntry;
typedef CMapWithOriginalOrderingIteration< CRef<CSeq_annot>, CSeq_entry_Handle> TMapSeqAnnotToDest;
typedef vector<CSeq_annot_Handle> TVecOfSeqAnnotsToErase;

typedef vector< CConstRef<CSeq_align> > TAlignVec;
typedef CMapWithOriginalOrderingIteration<CSeq_entry_Handle, TAlignVec> TMapEntryToAlignVec;

// returns true if any align was changed or deleted
static bool s_DivvyUpAlignments_ProcessAnnot_Dendiag(
    const CSeq_align_Handle & align,
    const TMapDescendentToInputEntry & mapDescendentToInputEntry,
    TMapEntryToAlignVec & mapEntryToAlignVec)
{
    bool bAnyAlignChanged = false;

    CScope & scope = align.GetScope();
    const CSeq_entry_Handle & old_input_entry =
        align.GetAnnot().GetParentEntry();

    // figure out which entry each dense diag goes into
    typedef vector< CConstRef<CDense_diag> > TDenseDiagVec;
    typedef map<CSeq_entry_Handle, TDenseDiagVec > TMapEntryToDenseDiags;
    // an unset CSeq_entry_Handle as key means to place them "everywhere".
    // all non-deleted dense_diags should be in some value somewhere in this map
    TMapEntryToDenseDiags mapEntryToDenseDiags;
    ITERATE( CSeq_align_Base::C_Segs::TDendiag,
        dendiag_iter,
        align.GetSegs().GetDendiag() )
    {
        CConstRef<CDense_diag> pDendiag = *dendiag_iter;
        if( FIELD_EQUALS(*pDendiag, Dim, 2) && 
            pDendiag->IsSetIds() && pDendiag->GetIds().size() == 2 ) 
        {
            // figure out which input entry this belongs to
            // (empty handle for "all")
            // If it belongs to multiple input entries, 
            // then it belongs *nowhere*, and we set bRemoveDendiag to true
            CSeq_entry_Handle dest_input_entry;
            bool bRemoveDendiag = false;
            ITERATE(CDense_diag_Base::TIds, id_iter, pDendiag->GetIds() ) {
                CBioseq_Handle bioseq = scope.GetBioseqHandle(**id_iter);
                CSeq_entry_Handle bioseqs_entry = (
                    bioseq ?
                    bioseq.GetParentEntry() :
                CSeq_entry_Handle() );
                TMapDescendentToInputEntry::const_iterator find_input_entry_iter = (
                    bioseqs_entry ?
                    mapDescendentToInputEntry.find(bioseqs_entry) :
                mapDescendentToInputEntry.end() );
                if( find_input_entry_iter == mapDescendentToInputEntry.end() ) {
                    continue;
                }

                CSeq_entry_Handle candidate_input_entry = 
                    find_input_entry_iter->second;
                _ASSERT(candidate_input_entry);

                // update dest_input_entry based on candidate_input_entry
                if( ! dest_input_entry ) {
                    // not set before, so set it now
                    dest_input_entry = candidate_input_entry;
                } else if( dest_input_entry == candidate_input_entry ) {
                    // great, matches so far
                } else {
                    // conflict: this align belongs on multiple seq-entries so it
                    // can't be put anywhere.  We note that it should be
                    // destroyed.
                    bRemoveDendiag = true;
                    break;
                }
            } // <-- ITERATE ids on dendiag

            if( bRemoveDendiag ) {
                bAnyAlignChanged = true;
                // don't add it to mapEntryToDenseDiags so it will eventually be lost
            } else {
                if( dest_input_entry != old_input_entry ) {
                    bAnyAlignChanged = true;
                }
                mapEntryToDenseDiags[dest_input_entry].push_back(pDendiag);
            }
        }  // <-- if dendiag is of dimensionality 2
        else {
            // dendiags of other dimensionality stay with same seq-entry
            mapEntryToDenseDiags[old_input_entry].push_back(pDendiag);
        }
    } // <-- ITERATE all dendiags on this alignment

    // first, check if we're in the (hopefully common) easy case of
    // "they all move to the same spot"
    if( mapEntryToDenseDiags.size() == 1 )
    {
        CSeq_entry_Handle dest_input_entry = 
            mapEntryToDenseDiags.begin()->first;
        mapEntryToAlignVec[dest_input_entry].push_back( align.GetSeq_align() );
    } else {
        // each moves to a different spot and some might even be deleted,
        // so we will have to copy the original align and break it into pieces

        NON_CONST_ITERATE( TMapEntryToDenseDiags, 
            entry_to_dendiags_iter,
            mapEntryToDenseDiags ) 
        {
            const CSeq_entry_Handle & dest_input_entry =
                entry_to_dendiags_iter->first;
            TDenseDiagVec & dendiag_vec = entry_to_dendiags_iter->second;
            _ASSERT( ! dendiag_vec.empty() );

            // copy the alignment for this dest_input_entry
            CRef<CSeq_align> pNewAlign( new CSeq_align );
            pNewAlign->Assign( *align.GetSeq_align() );
            pNewAlign->ResetSegs();

            CSeq_align::C_Segs::TDendiag & new_dendiag_vec =
                pNewAlign->SetSegs().SetDendiag();
            ITERATE( TDenseDiagVec, dendiag_vec_it, dendiag_vec ) {
                CRef<CDense_diag> pNewDendiag( new CDense_diag );
                pNewDendiag->Assign( **dendiag_vec_it );
                new_dendiag_vec.push_back( pNewDendiag );
            }

            mapEntryToAlignVec[dest_input_entry].push_back( pNewAlign );
        }

        bAnyAlignChanged = true;
    }

    return bAnyAlignChanged;
}

// returns true if any align was changed or deleted
static bool s_DivvyUpAlignments_ProcessAnnot_Denseg(
    const CSeq_align_Handle & align,
    const TMapDescendentToInputEntry & mapDescendentToInputEntry,
    TMapEntryToAlignVec & mapEntryToAlignVec)
{
    bool bAnyAlignChanged = false;

    CScope & scope = align.GetScope();
    const CSeq_entry_Handle & old_input_entry =
        align.GetAnnot().GetParentEntry();

    typedef vector<CDense_seg::TDim> TRowVec; // each element is a row (index into denseg)
    typedef map<CSeq_entry_Handle, TRowVec > TMapInputEntryToDensegRows;
    // this will map each input_entry to the rows in the denseg that it should use.
    // an empty key means "copy to every input entry"
    // (every non-deleted row index should have a value somewhere in mapInputEntryToDensegRows)
    TMapInputEntryToDensegRows mapInputEntryToDensegRows;

    // figure out what input entry each row belongs to
    const CDense_seg::TIds & ids = align.GetSegs().GetDenseg().GetIds();
    for( size_t iRow = 0; iRow < ids.size(); ++iRow ) {
        CBioseq_Handle id_bioseq = scope.GetBioseqHandle(*ids[iRow]);
        CSeq_entry_Handle id_bioseq_entry = 
            ( id_bioseq ? 
            id_bioseq.GetParentEntry() : 
        CSeq_entry_Handle() );
        TMapDescendentToInputEntry::const_iterator find_input_entry_iter =
            ( id_bioseq_entry ?
            mapDescendentToInputEntry.find(id_bioseq_entry) :
        mapDescendentToInputEntry.end() );
        if( find_input_entry_iter == mapDescendentToInputEntry.end() ) {
            // goes to all rows
            bAnyAlignChanged = true;
            mapInputEntryToDensegRows[CSeq_entry_Handle()].push_back(iRow);
            continue;
        }

        const CSeq_entry_Handle & id_input_entry = 
            find_input_entry_iter->second;
        _ASSERT(id_input_entry);

        if( id_input_entry != old_input_entry ) {
            bAnyAlignChanged = true;
        }
        mapInputEntryToDensegRows[id_input_entry].push_back(iRow);
    }

    if( ! bAnyAlignChanged ) {
        // easy case
        mapEntryToAlignVec[old_input_entry].push_back( align.GetSeq_align() );
    } else {
        // each row may end up in a different seq-entry

        ITERATE(TMapInputEntryToDensegRows, 
            input_entry_to_denseg_it, 
            mapInputEntryToDensegRows) 
        {
            const CSeq_entry_Handle & dest_input_entry = 
                input_entry_to_denseg_it->first;
            const TRowVec & rowVec = input_entry_to_denseg_it->second;

            // C++ toolkit has a handy function just for this purpose
            // (Note that it doesn't copy scores)
            CRef<CDense_seg> pNewDenseg = 
                align.GetSegs().GetDenseg().ExtractRows(rowVec);
            CRef<CSeq_align> pNewSeqAlign( new CSeq_align );
            pNewSeqAlign->Assign( *align.GetSeq_align() );
            pNewSeqAlign->SetSegs().SetDenseg( *pNewDenseg );

            mapEntryToAlignVec[dest_input_entry].push_back( pNewSeqAlign );

        }

        bAnyAlignChanged = true;
    }

    return bAnyAlignChanged;
}

static void s_DivvyUpAlignments_ProcessAnnot(
    const CSeq_annot_Handle & annot_h,
    const TMapDescendentToInputEntry & mapDescendentToInputEntry,
    TMapSeqAnnotToDest & mapSeqAnnotToDest,
    TVecOfSeqAnnotsToErase & vecOfSeqAnnotToErase )
{
    if( ! annot_h.IsAlign() ) {
        return;
    }

    CConstRef<CSeq_annot> pAnnot = annot_h.GetCompleteSeq_annot();
    if( ! pAnnot ) {
        // shouldn't happen
        return;
    }

    // figure out where each align should go.  If the key
    // is the empty CSeq_entry_Handle, that means it
    // goes on all input entries.

    // all non-deleted aligns should be in some value somewhere in this map
    TMapEntryToAlignVec mapEntryToAlignVec;

    // any change at all implies that we have to remove annot_h,
    // because it's going to be copied (or at least moved)
    bool bAnyAlignNeedsChange = false;

    CSeq_entry_Handle old_input_entry = annot_h.GetParentEntry();
    CAlign_CI align_ci(annot_h);
    for( ; align_ci; ++align_ci) {
        CSeq_align_Handle align = align_ci.GetSeq_align_Handle();

        const CSeq_align::TSegs & segs = align.GetSegs();

        if( segs.IsDendiag() ) {

            if( s_DivvyUpAlignments_ProcessAnnot_Dendiag(
                align, mapDescendentToInputEntry, mapEntryToAlignVec) ) 
            {
                bAnyAlignNeedsChange = true;
            }

        } else if( segs.IsDenseg() && 
            ! RAW_FIELD_IS_EMPTY(segs.GetDenseg(), Ids)  ) 
        {
            if( s_DivvyUpAlignments_ProcessAnnot_Denseg(
                align, mapDescendentToInputEntry, mapEntryToAlignVec) ) 
            {
                bAnyAlignNeedsChange = true;
            }

        } else {
            // other types stay on the same seq-entry
            mapEntryToAlignVec[old_input_entry].push_back(align.GetSeq_align());
        }
    } // <-- ITERATE through alignments on annot

    // check for the (hopefully common) easy case 
    // where we don't have to move the annot at all
    if( ! bAnyAlignNeedsChange ) {
        // easy: nothing to do
        return;
    }
    
        
    // use this as a template so we don't have to repeatedly
    // copy the whole annot and erase its aligns
    CRef<CSeq_annot> pOldAnnotWithNoAligns( new CSeq_annot );
    pOldAnnotWithNoAligns->Assign( *annot_h.GetCompleteSeq_annot() );
    pOldAnnotWithNoAligns->SetData().SetAlign().clear();

    // for each destination input entry, fill in mapSeqAnnotToDest
    // with a copy of seq-annot that just includes the aligns we care about
    ITERATE( TMapEntryToAlignVec::TKeyVec, 
        entry_to_aligns_iter, 
        mapEntryToAlignVec.GetKeysInOriginalOrder() )
    {
        const CSeq_entry_Handle & dest_input_entry = *entry_to_aligns_iter;
        TAlignVec & aligns_to_copy = 
            mapEntryToAlignVec.find(dest_input_entry)->second;

        // make copy of annot without aligns, but then
        // add the aligns that are relevant to this dest input entry
        CRef<CSeq_annot> pNewAnnot( new CSeq_annot );
        pNewAnnot->Assign(*pOldAnnotWithNoAligns);
        CSeq_annot::C_Data::TAlign & new_aligns = 
            pNewAnnot->SetData().SetAlign();

        _ASSERT( new_aligns.empty() );

        ITERATE( TAlignVec, align_it, aligns_to_copy ) {
            CRef<CSeq_align> pNewAlign( new CSeq_align );
            pNewAlign->Assign( **align_it );
            new_aligns.push_back( pNewAlign );
        }

        mapSeqAnnotToDest[pNewAnnot] = dest_input_entry;
    }

    // erase the old annot, since we made a copy
    vecOfSeqAnnotToErase.push_back(annot_h);
}

void DivvyUpAlignments(const TVecOfSeqEntryHandles & vecOfSeqEntryHandles)
{
    // create a mapping from all descendents of each member of 
    // vecOfSeqEntryHandles to that member.
    TMapDescendentToInputEntry mapDescendentToInputEntry;
    ITERATE(TVecOfSeqEntryHandles, input_entry_iter, vecOfSeqEntryHandles) {
        // it maps to itself, of course
        mapDescendentToInputEntry[*input_entry_iter] = *input_entry_iter;
        CSeq_entry_CI descendent_entry_iter(*input_entry_iter, CSeq_entry_CI::eRecursive);
        for(; descendent_entry_iter; ++descendent_entry_iter ) {
            mapDescendentToInputEntry[*descendent_entry_iter] = *input_entry_iter;
        }
    }

    // This mapping will hold the destination of each Seq_annot that
    // should be moved.  An empty destination handle 
    // means "copy to all members of vecOfSeqEntryHandles"
    // (read that carefully: one code path moves and the other copies.)
    TMapSeqAnnotToDest mapSeqAnnotToDest;

    // this holds the Seq-aligns that we will have to destroy.
    TVecOfSeqAnnotsToErase vecOfSeqAnnotsToErase;

    ITERATE(TVecOfSeqEntryHandles, input_entry_iter, vecOfSeqEntryHandles) {
        const CSeq_entry_Handle & input_entry_h = *input_entry_iter;
        CSeq_annot_CI annot_ci(input_entry_h, CSeq_annot_CI::eSearch_entry );
        for( ; annot_ci; ++annot_ci ) {
            s_DivvyUpAlignments_ProcessAnnot(
                *annot_ci,
                mapDescendentToInputEntry,
                mapSeqAnnotToDest,
                vecOfSeqAnnotsToErase );
        }
    }

    // do all the moves and copies that were requested
    ITERATE(TMapSeqAnnotToDest::TKeyVec, 
        annot_move_iter, 
        mapSeqAnnotToDest.GetKeysInOriginalOrder() ) 
    {
        CRef<CSeq_annot> pAnnot = *annot_move_iter;
        const CSeq_entry_Handle & dest_entry_h = mapSeqAnnotToDest.find(pAnnot)->second;

        // careful: one code path moves and the other copies.
        if( dest_entry_h ) {
            dest_entry_h.GetEditHandle().AttachAnnot(*pAnnot);
        } else {
            // if dest_entry_h is invalid, that means to copy
            // the annot to all
            ITERATE(TVecOfSeqEntryHandles, input_entry_iter, vecOfSeqEntryHandles) {
                CRef<CSeq_annot> pAnnotCopy( new CSeq_annot );
                pAnnotCopy->Assign(*pAnnot);
                input_entry_iter->GetEditHandle().AttachAnnot(*pAnnotCopy);
            }
        }
    }

    // erase the annots that were requested to be deleted
    ITERATE( TVecOfSeqAnnotsToErase, annot_iter, vecOfSeqAnnotsToErase ) {
        annot_iter->GetEditHandle().Remove();
    }
}

void BioseqSetDescriptorPropagateDown(
    const CBioseq_set_Handle & bioseq_set_h,
    const vector<CSeqdesc::E_Choice> &choices_to_delete )
{
    if( ! bioseq_set_h ) {
        return;
    }

    // sort it so we can use binary search on it
    CSeqdesc_CI::TDescChoices sorted_choices_to_delete = choices_to_delete;
    stable_sort( sorted_choices_to_delete.begin(),
                 sorted_choices_to_delete.end() );

    // retrieve all the CSeqdescs that we will have to copy
    // (if a Seqdesc isn't copied into here, it's implicitly
    // deleted )
    CConstRef<CSeq_descr> pSeqDescrToCopy;
    {
        // we have this pSeqDescrWithChosenDescs variable because 
        // we want pSeqDescrToCopy to be protected
        // once it's set
        CRef<CSeq_descr> pSeqDescrWithChosenDescs( new CSeq_descr );
        CSeqdesc_CI desc_ci( bioseq_set_h.GetParentEntry(), CSeqdesc::e_not_set, 1);
        for( ; desc_ci; ++desc_ci ) {
            if( ! binary_search( sorted_choices_to_delete.begin(),
                sorted_choices_to_delete.end(), desc_ci->Which() ) )
            {
                // not one of the deleted ones, so add it
                pSeqDescrWithChosenDescs->Set().push_back( 
                    CRef<CSeqdesc>( SerialClone(*desc_ci) ) );
            }
        }
        pSeqDescrToCopy = pSeqDescrWithChosenDescs;
    }

    // copy to all immediate children
    CSeq_entry_CI direct_child_ci( bioseq_set_h, CSeq_entry_CI::eNonRecursive );
    for( ; direct_child_ci; ++direct_child_ci ) {
        CRef<CSeq_descr> pNewDescr( SerialClone(*pSeqDescrToCopy) );
        direct_child_ci->GetEditHandle().AddDescr( 
            *SerialClone(*pSeqDescrToCopy) );
    }

    // remove all descs from the parent
    bioseq_set_h.GetEditHandle().ResetDescr();
}


void AddLocalIdUserObjects(CSeq_entry& entry)
{
    if (entry.IsSeq()) {
        bool need_object = true;
        CBioseq& seq = entry.SetSeq();
        if (seq.IsSetDescr()) {
            ITERATE(CBioseq::TDescr::Tdata, d, seq.GetDescr().Get()) {
                if ((*d)->IsUser()
                    && (*d)->GetUser().GetObjectType() == CUser_object::eObjectType_OriginalId) {
                    need_object = false;
                    break;
                }
            }
        }
        if (need_object) {
            CRef<CUser_object> obj(new CUser_object());
            obj->SetObjectType(CUser_object::eObjectType_OriginalId);
            ITERATE(CBioseq::TId, id, entry.GetSeq().GetId()) {
                if ((*id)->IsLocal()) {
                    string val = "";
                    if ((*id)->GetLocal().IsStr()) {
                        val = (*id)->GetLocal().GetStr();
                    } else if ((*id)->GetLocal().IsId()) {
                        val = NStr::NumericToString((*id)->GetLocal().GetId());
                    }
                    if (!NStr::IsBlank(val)) {
                        CRef<CUser_field> field(new CUser_field());
                        field->SetLabel().SetStr("LocalId");
                        field->SetData().SetStr(val);
                        obj->SetData().push_back(field);
                    }
                }
            }
            if (obj->IsSetData()) {
                CRef<CSeqdesc> desc(new CSeqdesc());
                desc->SetUser(*obj);
                seq.SetDescr().Set().push_back(desc);
            }
        }
    } else if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {
        NON_CONST_ITERATE(CBioseq_set::TSeq_set, s, entry.SetSet().SetSeq_set()) {
            AddLocalIdUserObjects(**s);
        }
    }
}


bool HasRepairedIDs(const CUser_object& user, const CBioseq::TId& ids)
{
    bool rval = false;
    if (user.IsSetData()) {
        ITERATE(CUser_object::TData, it, user.GetData()) {
            if ((*it)->IsSetLabel() && (*it)->GetLabel().IsStr()
                && NStr::EqualNocase((*it)->GetLabel().GetStr(), "LocalId")
                && (*it)->IsSetData()
                && (*it)->GetData().IsStr()) {
                string orig_id = (*it)->GetData().GetStr();
                bool found = false;
                bool any_local = false;
                ITERATE(CBioseq::TId, id_it, ids) {
                    if ((*id_it)->IsLocal()) {
                        any_local = true;
                        if ((*id_it)->GetLocal().IsStr()
                            && NStr::EqualNocase((*id_it)->GetLocal().GetStr(), orig_id)) {
                            found = true;
                        } else if ((*id_it)->GetLocal().IsId()
                            && NStr::EqualNocase(NStr::NumericToString((*id_it)->GetLocal().GetId()), orig_id)) {
                            found = true;
                        }
                    }
                }
                if (any_local && !found) {
                    rval = true;
                    break;
                }
            }
        }
    }
    return rval;
}


bool HasRepairedIDs(const CSeq_entry& entry)
{
    bool rval = false;
    if (entry.IsSeq()) {
        const CBioseq& seq = entry.GetSeq();
        if (seq.IsSetDescr() && seq.IsSetId()) {
            ITERATE(CBioseq::TDescr::Tdata, d, seq.GetDescr().Get()) {
                if ((*d)->IsUser()
                    && (*d)->GetUser().GetObjectType() == CUser_object::eObjectType_OriginalId) {
                    rval = HasRepairedIDs((*d)->GetUser(), seq.GetId());
                    if (rval) {
                        break;
                    }
                }
            }
        }
    } else if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {
        ITERATE(CBioseq_set::TSeq_set, s, entry.GetSet().GetSeq_set()) {
            rval = HasRepairedIDs(**s);
            if (rval) {
                break;
            }
        }
    }
    return rval;
}


void s_AddLiteral(CSeq_inst& inst, const string& element)
{
    CRef<CDelta_seq> ds(new CDelta_seq());
    ds->SetLiteral().SetSeq_data().SetIupacna().Set(element);
    ds->SetLiteral().SetLength(element.length());
    
    inst.SetExt().SetDelta().Set().push_back(ds);
}


void s_AddGap(CSeq_inst& inst, size_t n_len, bool is_unknown, bool is_assembly_gap = false, int gap_type = CSeq_gap::eType_unknown, int linkage = -1, int linkage_evidence = -1 )
{
    CRef<CDelta_seq> gap(new CDelta_seq());
    if (is_assembly_gap)
    {
        gap->SetLiteral().SetSeq_data().SetGap();
        gap->SetLiteral().SetSeq_data().SetGap().SetType(gap_type);
        if (linkage >= 0)
        {
            gap->SetLiteral().SetSeq_data().SetGap().SetLinkage(linkage);
        }
        if (linkage_evidence >= 0)
        {
            CRef<CLinkage_evidence> link_ev(new CLinkage_evidence);
            link_ev->SetType(linkage_evidence);
            gap->SetLiteral().SetSeq_data().SetGap().SetLinkage_evidence().push_back(link_ev);
        }
    }
    if (is_unknown) {
        gap->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
    }
    gap->SetLiteral().SetLength(n_len);
    inst.SetExt().SetDelta().Set().push_back(gap);
}


/// ConvertRawToDeltaByNs
/// A function to convert a raw sequence to a delta sequence, using runs of
/// Ns to determine the gap location. The size of the run of Ns determines
/// whether a gap should be created and whether the gap should be of type
/// known or unknown. Note that if the ranges overlap, unknown gaps will be
/// preferred (allowing the user to create known length gaps for 20-forever,
/// but unknown length gaps for 100, for example).
/// Use a negative number for a maximum to indicate that there is no upper
/// limit.
/// @param inst        The Seq-inst to adjust
/// @param min_unknown The minimum number of Ns to be converted to a gap of 
///                    unknown length
/// @param max_unknown The maximum number of Ns to be converted to a gap of 
///                    unknown length
/// @param min_known   The minimum number of Ns to be converted to a gap of 
///                    known length
/// @param max_known   The maximum number of Ns to be converted to a gap of 
///                    known length
///
/// @return            none
void ConvertRawToDeltaByNs(CSeq_inst& inst, 
                           size_t min_unknown, int max_unknown, 
                           size_t min_known,   int max_known)
{
    // can only convert if starting as raw
    if (!inst.IsSetRepr() || inst.GetRepr() != CSeq_inst::eRepr_raw
        || !inst.IsSetSeq_data()) {
        return;
    }

    string iupacna;

    switch(inst.GetSeq_data().Which()) {
        case CSeq_data::e_Iupacna:
            iupacna = inst.GetSeq_data().GetIupacna();
            break;
        case CSeq_data::e_Ncbi2na:
            CSeqConvert::Convert(inst.GetSeq_data().GetNcbi2na().Get(), CSeqUtil::e_Ncbi2na,
                                    0, inst.GetLength(), iupacna, CSeqUtil::e_Iupacna);
            break;
        case CSeq_data::e_Ncbi4na:
            CSeqConvert::Convert(inst.GetSeq_data().GetNcbi4na().Get(), CSeqUtil::e_Ncbi4na,
                                    0, inst.GetLength(), iupacna, CSeqUtil::e_Iupacna);
            break;
        case CSeq_data::e_Ncbi8na:
            CSeqConvert::Convert(inst.GetSeq_data().GetNcbi8na().Get(), CSeqUtil::e_Ncbi8na,
                                    0, inst.GetLength(), iupacna, CSeqUtil::e_Iupacna);
            break;
        default:
            return;
            break;
    }
  
    string element = "";
    size_t n_len = 0;
    ITERATE(string, it, iupacna) {
        if ((*it) == 'N') {
            n_len++;
            element += *it;
        } else {
            if (n_len > 0) {
                // decide whether to turn this past run of Ns into a gap
                bool is_unknown = false;
                bool is_known = false;

                if (n_len >= min_unknown && (max_unknown < 0 || n_len <= max_unknown)) {
                    is_unknown = true;
                } else if (n_len >= min_known && (max_known < 0 || n_len <= max_known)) {
                    is_known = true;
                }
                if (is_unknown || is_known) {
                   // make literal to contain sequence before gap
                    if (element.length() > n_len) {
                        element = element.substr(0, element.length() - n_len);
                        s_AddLiteral(inst, element);
                    }
                    s_AddGap(inst, n_len, is_unknown);
                    element = "";
                }
                n_len = 0;
            }
            element += *it;
        }
    }

    if (n_len > 0) {
        // decide whether to turn this past run of Ns into a gap
        bool is_unknown = false;
        bool is_known = false;

        if (n_len >= min_unknown && (max_unknown < 0 || n_len <= max_unknown)) {
            is_unknown = true;
        } else if (n_len >= min_known && (max_known < 0 || n_len <= max_known)) {
            is_known = true;
        }
        if (is_unknown || is_known) {
            // make literal to contain sequence before gap
            if (element.length() > n_len) {
                element = element.substr(0, element.length() - n_len);
                s_AddLiteral(inst, element);
            }
            s_AddGap(inst, n_len, is_unknown);
        }
    } else {
        s_AddLiteral(inst, element);
    }

    inst.SetRepr(CSeq_inst::eRepr_delta);
    inst.ResetSeq_data();
}


/// NormalizeUnknownLengthGaps
/// A function to adjust the length of unknown-length gaps to a specific
/// length (100 by default).
/// @param inst        The Seq-inst to adjust
///
/// @return            A vector of the adjustments to the sequence,
///                    which can be used to fix the locations of features
///                    on the sequence.
TLocAdjustmentVector NormalizeUnknownLengthGaps(CSeq_inst& inst, size_t unknown_length)
{
    TLocAdjustmentVector changes;

    // can only adjust if starting as delta sequence
    if (!inst.IsSetRepr() || inst.GetRepr() != CSeq_inst::eRepr_delta
        || !inst.IsSetExt()) {
        return changes;
    }

    size_t pos = 0;
    NON_CONST_ITERATE(CSeq_ext::TDelta::Tdata, it, inst.SetExt().SetDelta().Set()) {
        size_t orig_len = 0;
        if ((*it)->IsLiteral()) {
            if ((*it)->GetLiteral().IsSetLength()) {
                orig_len = (*it)->GetLiteral().GetLength();
            }            
            if ((*it)->GetLiteral().IsSetFuzz()
                && orig_len != unknown_length
                && (!(*it)->GetLiteral().IsSetSeq_data() || (*it)->GetLiteral().GetSeq_data().IsGap())) {                

                int diff = unknown_length - orig_len;
                (*it)->SetLiteral().SetLength(unknown_length);
                changes.push_back(TLocAdjustment(pos, diff));
                inst.SetLength(inst.GetLength() + diff);
            }
        } else if ((*it)->IsLoc()) {
            orig_len = (*it)->GetLoc().GetTotalRange().GetLength();
        }

        pos += orig_len;
    }

    return changes;
}


void ConvertRawToDeltaByNs(CBioseq_Handle bsh, 
                           size_t min_unknown, int max_unknown, 
                           size_t min_known, int max_known)
{
    CRef<CSeq_inst> inst(new CSeq_inst());
    inst->Assign(bsh.GetInst());

    ConvertRawToDeltaByNs(*inst, min_unknown, max_unknown, min_known, max_known);
    TLocAdjustmentVector changes = NormalizeUnknownLengthGaps(*inst);
    CBioseq_EditHandle beh = bsh.GetEditHandle();
    beh.SetInst(*inst);

    if (changes.size() > 0) {
        for (CFeat_CI f(bsh); f; ++f) {
            CRef<CSeq_feat> cpy(new CSeq_feat());
            cpy->Assign(*(f->GetSeq_feat()));
            TLocAdjustmentVector::reverse_iterator it = changes.rbegin();
            bool cut = false;
            bool trimmed = false;
            while (it != changes.rend() && !cut) {
                if (it->second < 0) {
                    FeatureAdjustForTrim(*cpy, it->first, it->first - it->second + 1, NULL, cut, trimmed);
                } else {
                    FeatureAdjustForInsert(*cpy, it->first, it->first + it->second - 1, NULL);
                }
                it++;
            }
            CSeq_feat_EditHandle feh(f->GetSeq_feat_Handle());
            if (cut) {
                feh.Remove();
            } else {
                feh.Replace(*cpy);
            }
        }
    }

}


/// SetLinkageType
/// A function to set the linkage_type for gaps in a delta sequence.
/// @param ext          The Seq_ext to adjust
/// @param linkage_type The linkage_type to use.
///
/// @return            none
void SetLinkageType(CSeq_ext& ext, CSeq_gap::TType linkage_type)
{
    NON_CONST_ITERATE(CSeq_ext::TDelta::Tdata, it, ext.SetDelta().Set()) {
        if ((*it)->IsLiteral()
            && (!(*it)->GetLiteral().IsSetSeq_data() || (*it)->GetLiteral().GetSeq_data().IsGap())) {
            CSeq_gap& gap = (*it)->SetLiteral().SetSeq_data().SetGap();
            gap.ChangeType(linkage_type);
        }
    }
}


/// SetLinkageTypeScaffold
/// A special case of SetLinkageType. When type is Scaffold, linkage must be
/// linked and linkage evidence must be provided.
/// @param ext           The Seq_ext to adjust
/// @param evidence_type The linkage_type to use.
///
/// @return            none
void SetLinkageTypeScaffold(CSeq_ext& ext, CLinkage_evidence::TType evidence_type)
{
    NON_CONST_ITERATE(CSeq_ext::TDelta::Tdata, it, ext.SetDelta().Set()) {
        if ((*it)->IsLiteral()
            && (!(*it)->GetLiteral().IsSetSeq_data() || (*it)->GetLiteral().GetSeq_data().IsGap())) {
            CSeq_gap& gap = (*it)->SetLiteral().SetSeq_data().SetGap();
            gap.SetLinkageTypeScaffold(evidence_type);
        }
    }
}


void SetLinkageTypeLinkedRepeat(CSeq_ext& ext, CLinkage_evidence::TType evidence_type)
{
    NON_CONST_ITERATE(CSeq_ext::TDelta::Tdata, it, ext.SetDelta().Set()) {
        if ((*it)->IsLiteral()
            && (!(*it)->GetLiteral().IsSetSeq_data() || (*it)->GetLiteral().GetSeq_data().IsGap())) {
            CSeq_gap& gap = (*it)->SetLiteral().SetSeq_data().SetGap();
            gap.SetLinkageTypeLinkedRepeat(evidence_type);
        }
    }
}


/// AddLinkageEvidence
/// A function to add linkage evidence for gaps in a delta sequence.
/// Note that this function will automatically set the linkage to eLinkage_linked.
/// @param ext           The Seq_ext to adjust
/// @param evidence_type The evidence type to use.
///
/// @return            none
void AddLinkageEvidence(CSeq_ext& ext, CLinkage_evidence::TType evidence_type)
{
    NON_CONST_ITERATE(CSeq_ext::TDelta::Tdata, it, ext.SetDelta().Set()) {
        if ((*it)->IsLiteral()
            && (!(*it)->GetLiteral().IsSetSeq_data() || (*it)->GetLiteral().GetSeq_data().IsGap())) {
            CSeq_gap& gap = (*it)->SetLiteral().SetSeq_data().SetGap();
            gap.AddLinkageEvidence(evidence_type);
        }
    }
}


/// ResetLinkageEvidence
/// A function to clear linkage evidence for gaps in a delta sequence.
/// @param ext           The Seq_ext to adjust
///
/// @return            none
void ResetLinkageEvidence(CSeq_ext& ext)
{
    NON_CONST_ITERATE(CSeq_ext::TDelta::Tdata, it, ext.SetDelta().Set()) {
        if ((*it)->IsLiteral()
            && (!(*it)->GetLiteral().IsSetSeq_data() || (*it)->GetLiteral().GetSeq_data().IsGap())) {
            CSeq_gap& gap = (*it)->SetLiteral().SetSeq_data().SetGap();
            if (gap.IsSetType() && gap.GetType() == CSeq_gap::eType_repeat) {
                gap.SetLinkage(CSeq_gap::eLinkage_unlinked);
            } else {
                gap.ResetLinkage();
            }
            gap.ResetLinkage_evidence();
        }
    }
}


/*******************************************************************************
**** HIGH-LEVEL API
****
**** Trim functions
*******************************************************************************/

void s_BasicValidation(CBioseq_Handle bsh, 
                       const TCuts& cuts)
{
    // Should be a nuc!
    if (!bsh.IsNucleotide()) {
        NCBI_THROW(CEditException, eInvalid, "Bioseq is not a nucleotide.");
    }

    // Cannot get nuc sequence data
    if (!bsh.CanGetInst()) {
        NCBI_THROW(CEditException, eInvalid, "Cannot get sequence data for nucleotide.");
    }

    // Are the cuts within range of sequence length?
    TSeqPos nuc_len = 0;
    if (bsh.GetInst().CanGetLength()) {
        nuc_len = bsh.GetInst().GetLength();
    }

    if (nuc_len <= 0) {
        stringstream ss;
        ss << "Nuc has invalid sequence length = " << nuc_len;
        NCBI_THROW(CEditException, eInvalid, ss.str());
    }

    TCuts::const_iterator cit;
    for (cit = cuts.begin(); cit != cuts.end(); ++cit) {
        const TRange& cut = *cit;
        TSeqPos cut_from = cut.GetFrom();
        TSeqPos cut_to = cut.GetTo();
        if (cut_from < 0 || cut_to < 0 || cut_from >= nuc_len || cut_to >= nuc_len) {
            stringstream ss;
            ss << "Cut location is invalid = [" << cut_from << " - " << cut_to << "]";
            NCBI_THROW(CEditException, eInvalid, ss.str());
        }
    }
}


/// Implementation detail: first trim all associated annotation, then 
/// trim sequence data
void TrimSequenceAndAnnotation(CBioseq_Handle bsh, 
                               const TCuts& cuts,
                               EInternalTrimType internal_cut_conversion)
{
    // Check the input data for anomalies
    s_BasicValidation(bsh, cuts);

    // Sort the cuts 
    TCuts sorted_cuts;
    GetSortedCuts(bsh, cuts, sorted_cuts, internal_cut_conversion);

    // Trim a copy of seq_inst but don't update the original seq_inst just yet.
    // Do the update as the last step after trimming all annotation first.
    // I need the trimmed seq_inst when I retranslate a protein sequence.
    // Make a copy of seq_inst
    CRef<CSeq_inst> copy_inst(new CSeq_inst());
    copy_inst->Assign(bsh.GetInst());
    // Modify the copy of seq_inst
    TrimSeqData(bsh, copy_inst, sorted_cuts);

    // Trim Seq-feat annotation
    SAnnotSelector feat_sel(CSeq_annot::C_Data::e_Ftable);
    CFeat_CI feat_ci(bsh, feat_sel);
    for (; feat_ci; ++feat_ci) {
        // Make a copy of the feature
        CRef<CSeq_feat> copy_feat(new CSeq_feat());
        copy_feat->Assign(feat_ci->GetOriginalFeature());

        // Detect complete deletions of feature
        bool bFeatureDeleted = false;

        // Detect case where feature was not deleted but merely trimmed
        bool bFeatureTrimmed = false;

        // Modify the copy of the feature
        TrimSeqFeat(copy_feat, sorted_cuts, bFeatureDeleted, bFeatureTrimmed);

        if (bFeatureDeleted) {
            // Delete the feature
            // If the feature was a cdregion, delete the protein and
            // renormalize the nuc-prot set
            DeleteProteinAndRenormalizeNucProtSet(*feat_ci);
        }
        else 
        if (bFeatureTrimmed) {
            // Further modify the copy of the feature

            // If this feat is a Cdregion, then RETRANSLATE the protein
            // sequence AND adjust any protein feature
            if ( copy_feat->IsSetData() && 
                 copy_feat->GetData().Which() == CSeqFeatData::e_Cdregion &&
                 copy_feat->IsSetProduct() )
            {
                // Get length of nuc sequence before trimming
                TSeqPos original_nuc_len = 0;
                if (bsh.GetInst().CanGetLength()) {
                    original_nuc_len = bsh.GetInst().GetLength();
                }
                AdjustCdregionFrame(original_nuc_len, copy_feat, sorted_cuts);

                // Retranslate the coding region using the new nuc sequence
                RetranslateCdregion(bsh, copy_inst, copy_feat, sorted_cuts);
            }

            // Update the original feature with the modified copy
            CSeq_feat_EditHandle feat_eh(*feat_ci);
            feat_eh.Replace(*copy_feat);
        }
    }

    // Trim Seq-align annotation
    SAnnotSelector align_sel(CSeq_annot::C_Data::e_Align);
    CAlign_CI align_ci(bsh, align_sel);
    for (; align_ci; ++align_ci) {
        // Only DENSEG type is supported
        const CSeq_align& align = *align_ci;
        if ( align.CanGetSegs() && 
             align.GetSegs().Which() == CSeq_align::C_Segs::e_Denseg )
        {
            // Make sure mandatory fields are present in the denseg
            const CDense_seg& denseg = align.GetSegs().GetDenseg();
            if (! (denseg.CanGetDim() && denseg.CanGetNumseg() && 
                   denseg.CanGetIds() && denseg.CanGetStarts() &&
                   denseg.CanGetLens()) )
            {
                continue;
            }

            // Make a copy of the alignment
            CRef<CSeq_align> copy_align(new CSeq_align());
            copy_align->Assign(align_ci.GetOriginalSeq_align());

            // Modify the copy of the alignment
            TrimSeqAlign(bsh, copy_align, sorted_cuts);

            // Update the original alignment with the modified copy
            align_ci.GetSeq_align_Handle().Replace(*copy_align);
        }
    }

    // Trim Seq-graph annotation
    SAnnotSelector graph_sel(CSeq_annot::C_Data::e_Graph);
    CGraph_CI graph_ci(bsh, graph_sel);
    for (; graph_ci; ++graph_ci) {
        // Only certain types of graphs are supported.
        // See C Toolkit function GetGraphsProc in api/sqnutil2.c
        const CMappedGraph& graph = *graph_ci;
        if ( graph.IsSetTitle() && 
             (NStr::CompareNocase( graph.GetTitle(), "Phrap Quality" ) == 0 ||
              NStr::CompareNocase( graph.GetTitle(), "Phred Quality" ) == 0 ||
              NStr::CompareNocase( graph.GetTitle(), "Gap4" ) == 0) )
        {
            // Make a copy of the graph
            CRef<CSeq_graph> copy_graph(new CSeq_graph());
            copy_graph->Assign(graph.GetOriginalGraph());

            // Modify the copy of the graph
            TrimSeqGraph(bsh, copy_graph, sorted_cuts);

            // Update the original graph with the modified copy
            graph.GetSeq_graph_Handle().Replace(*copy_graph);
        }
    }

    // Last step - trim sequence data by updating the original seq_inst with the
    // modified copy
    bsh.GetEditHandle().SetInst(*copy_inst);
}


/*******************************************************************************
**** LOW-LEVEL API
****
**** Trim functions divided up into trimming separate distinct objects, i.e.,
**** the sequence data itself and all associated annotation.
****
**** Used by callers who need access to each edited object so that they can 
**** pass these edited objects to a command undo/redo framework, for example.
*******************************************************************************/

/// Helper functor to compare cuts during sorting
class CRangeCmp : public binary_function<TRange, TRange, bool>
{
public:
    enum ESortOrder {
        eAscending,
        eDescending
    };

    explicit CRangeCmp(ESortOrder sortorder = eAscending)
      : m_sortorder(sortorder) {};

    bool operator()(const TRange& a1, const TRange& a2) 
    {
        if (m_sortorder == eAscending) {
            if (a1.GetTo() == a2.GetTo()) {
                // Tiebreaker
                return a1.GetFrom() < a2.GetFrom();
            }
            return a1.GetTo() < a2.GetTo();
        }
        else {
            if (a1.GetTo() == a2.GetTo()) {
                // Tiebreaker
                return a1.GetFrom() > a2.GetFrom();
            }
            return a1.GetTo() > a2.GetTo();
        }
    };

private:
    ESortOrder m_sortorder;
};


/// Assumes sorted_cuts are sorted in Ascending order!
static void s_MergeCuts(TCuts& sorted_cuts)
{
    // Merge abutting and overlapping cuts
    TCuts::iterator it;
    for (it = sorted_cuts.begin(); it != sorted_cuts.end(); ) {
        TRange& cut = *it;
        TSeqPos to = cut.GetTo();

        // Does next cut exist?
        if ( it+1 != sorted_cuts.end() ) {
            TRange& next_cut = *(it+1);
            TSeqPos next_from = next_cut.GetFrom();
            TSeqPos next_to = next_cut.GetTo();

            if ( next_from <= (to + 1) ) {
                // Current and next cuts abut or overlap
                // So adjust current cut and delete next cut
                cut.SetTo(next_to);
                sorted_cuts.erase(it+1);

                // Post condition after erase:
                // Since "it" is before the erase, "it" stays valid
                // and still refers to current cut
            }
            else {
                ++it;
            }
        }
        else {
            // I'm done
            break;
        }
    }
}


/// Adjust any internal cuts to terminal cuts
static void s_AdjustInternalCutLocations(TCuts& cuts, 
                                         TSeqPos seq_length,
                                         EInternalTrimType internal_cut_conversion)
{
    for (TCuts::size_type ii = 0; ii < cuts.size(); ++ii) {
        TRange& cut = cuts[ii];
        TSeqPos from = cut.GetFrom();
        TSeqPos to = cut.GetTo();

        // Is it an internal cut?
        if (from != 0 && to != seq_length-1) {
            if (internal_cut_conversion == eTrimToClosestEnd) {
                // Extend the cut to the closest end
                if (from - 0 < seq_length-1 - to) {
                    cut.SetFrom(0);
                }
                else {
                    cut.SetTo(seq_length-1);
                }
            }
            else 
            if (internal_cut_conversion == eTrimTo5PrimeEnd) {
                // Extend the cut to 5' end
                cut.SetFrom(0);
            }
            else {
                // Extend the cut to 3' end
                cut.SetTo(seq_length-1);
            }
        }
    }
}


/// 1) Merge abutting and overlapping cuts.
/// 2) Adjust any internal cuts to terminal cuts according to option.
/// 3) Sort the cuts from greatest to least so that sequence
///    data and annotation will be deleted from greatest loc to smallest loc.
///    That way we don't have to adjust coordinate values after 
///    each cut.
void GetSortedCuts(CBioseq_Handle bsh, 
                   const TCuts& cuts,
                   TCuts& sorted_cuts,
                   EInternalTrimType internal_cut_conversion)
{
    sorted_cuts = cuts;

    /***************************************************************************
     * Merge abutting and overlapping cuts
     * Adjust internal cuts to terminal cuts
    ***************************************************************************/
    CRangeCmp asc(CRangeCmp::eAscending);
    sort(sorted_cuts.begin(), sorted_cuts.end(), asc);

    // Merge abutting and overlapping cuts
    s_MergeCuts(sorted_cuts);

    // Adjust internal cuts to terminal cuts
    s_AdjustInternalCutLocations(sorted_cuts, bsh.GetBioseqLength(), 
                                 internal_cut_conversion);


    /***************************************************************************
     * Sort the cuts in descending order
    ***************************************************************************/
    // Sort the ranges from greatest to least so that sequence
    // data and annotation will be deleted from greatest loc to smallest loc.
    // That way we don't have to adjust coordinate values after 
    // each delete.
    CRangeCmp descend(CRangeCmp::eDescending);
    sort(sorted_cuts.begin(), sorted_cuts.end(), descend);
}


/// Trim sequence data
void TrimSeqData(CBioseq_Handle bsh, 
                 CRef<CSeq_inst> inst, 
                 const TCuts& sorted_cuts)
{
    // Should be a nuc!
    if (!bsh.IsNucleotide()) {
        return;
    }

    // There should be sequence data
    if ( !(bsh.CanGetInst() && bsh.GetInst().IsSetSeq_data()) ) {
        return;
    }

    // Copy residues to buffer
    CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);
    string seq_string;
    vec.GetSeqData(0, vec.size(), seq_string);

    // Delete residues per cut
    for (TCuts::size_type ii = 0; ii < sorted_cuts.size(); ++ii) {
        const TRange& cut = sorted_cuts[ii];
        TSeqPos start = cut.GetFrom();
        TSeqPos length = cut.GetTo() - start + 1;
        seq_string.erase(start, length);
    }

    // Update sequence length and sequence data
    inst->SetLength(seq_string.size());
    inst->SetSeq_data().SetIupacna(*new CIUPACna(seq_string));
    CSeqportUtil::Pack(&inst->SetSeq_data());
}


static void s_GetTrimCoordinates(CBioseq_Handle bsh,
                                 const TCuts& sorted_cuts,
                                 TSeqPos& trim_start,
                                 TSeqPos& trim_stop)
{
    // Set defaults
    trim_start = 0;
    trim_stop = bsh.GetInst().GetLength() - 1;

    // Assumptions :
    // All cuts have been sorted.  Internal cuts were converted to terminal.
    for (TCuts::size_type ii = 0; ii < sorted_cuts.size(); ++ii) {
        const TRange& cut = sorted_cuts[ii];
        TSeqPos from = cut.GetFrom();
        TSeqPos to = cut.GetTo();

        // Left-side terminal cut.  Update trim_start if necessary.
        if ( from == 0 ) {
            if ( trim_start <= to ) {
                trim_start = to + 1;
            }
        }

        // Right-side terminal cut.  Update trim_stop if necessary.
        if ( to == bsh.GetInst().GetLength() - 1 ) {
            if ( trim_stop >= from ) {
                trim_stop = from - 1;
            }
        }
    }
}


static void s_SeqIntervalDelete(CRef<CSeq_interval> interval, 
                                TSeqPos cut_from,
                                TSeqPos cut_to,
                                bool& bCompleteCut,
                                bool& bTrimmed)
{
    // These are required fields
    if ( !(interval->CanGetFrom() && interval->CanGetTo()) ) {
        return;
    }

    // Feature location
    TSeqPos feat_from = interval->GetFrom();
    TSeqPos feat_to = interval->GetTo();

    // Size of the cut
    TSeqPos cut_size = cut_to - cut_from + 1;

    // Case 1: feature is located completely before the cut
    if (feat_to < cut_from)
    {
        // Nothing needs to be done - cut does not affect feature
        return;
    }

    // Case 2: feature is completely within the cut
    if (feat_from >= cut_from && feat_to <= cut_to)
    {
        // Feature should be deleted
        bCompleteCut = true;
        return;
    }

    // Case 3: feature is completely past the cut
    if (feat_from > cut_to)
    {
        // Shift the feature by the cut_size
        feat_from -= cut_size;
        feat_to -= cut_size;
        interval->SetFrom(feat_from);
        interval->SetTo(feat_to);
        bTrimmed = true;
        return;
    }

    /***************************************************************************
     * Cases below are partial overlapping cases
    ***************************************************************************/
    // Case 4: Cut is completely inside the feature 
    //         OR
    //         Cut is to the "left" side of the feature (i.e., feat_from is 
    //         inside the cut)
    //         OR
    //         Cut is to the "right" side of the feature (i.e., feat_to is 
    //         inside the cut)
    if (feat_to > cut_to) {
        // Left side cut or cut is completely inside feature
        feat_to -= cut_size;
    }
    else {
        // Right side cut
        feat_to = cut_from - 1;
    }

    // Take care of the feat_from from the left side cut case
    if (feat_from >= cut_from) {
        feat_from = cut_to + 1;
        feat_from -= cut_size;
    }

    interval->SetFrom(feat_from);
    interval->SetTo(feat_to);
    bTrimmed = true;
}


static void s_SeqLocDelete(CRef<CSeq_loc> loc, 
                           TSeqPos from, 
                           TSeqPos to,
                           bool& bCompleteCut,
                           bool& bTrimmed)
{
    // Given a seqloc and a range, cut the seqloc

    switch(loc->Which())
    {
        // Single interval
        case CSeq_loc::e_Int:
        {
            CRef<CSeq_interval> interval(new CSeq_interval);
            interval->Assign(loc->GetInt());
            s_SeqIntervalDelete(interval, from, to, bCompleteCut, bTrimmed);
            loc->SetInt(*interval);
        }
        break;

        // Multiple intervals
        case CSeq_loc::e_Packed_int:
        {
            CRef<CSeq_loc::TPacked_int> intervals(new CSeq_loc::TPacked_int);
            intervals->Assign(loc->GetPacked_int());
            if (intervals->CanGet()) {
                // Process each interval in the list
                CPacked_seqint::Tdata::iterator it;
                for (it = intervals->Set().begin(); 
                     it != intervals->Set().end(); ) 
                {
                    // Initial value: assume that all intervals 
                    // will be deleted resulting in bCompleteCut = true.
                    // Later on if any interval is not deleted, then set
                    // bCompleteCut = false
                    if (it == intervals->Set().begin()) {
                        bCompleteCut = true;
                    }

                    bool bDeleted = false;
                    s_SeqIntervalDelete(*it, from, to, bDeleted, bTrimmed);

                    // Should interval be deleted from list?
                    if (bDeleted) {
                        it = intervals->Set().erase(it);
                    }
                    else {
                        ++it;
                        bCompleteCut = false;
                    }
                }

                // Update the original list
                loc->SetPacked_int(*intervals);
            }
        }
        break;

        // Multiple seqlocs
        case CSeq_loc::e_Mix:
        {
            CRef<CSeq_loc_mix> mix(new CSeq_loc_mix);
            mix->Assign(loc->GetMix());
            if (mix->CanGet()) {
                // Process each seqloc in the list
                CSeq_loc_mix::Tdata::iterator it;
                for (it = mix->Set().begin(); 
                     it != mix->Set().end(); ) 
                {
                    // Initial value: assume that all seqlocs
                    // will be deleted resulting in bCompleteCut = true.
                    // Later on if any seqloc is not deleted, then set
                    // bCompleteCut = false
                    if (it == mix->Set().begin()) {
                        bCompleteCut = true;
                    }

                    bool bDeleted = false;
                    s_SeqLocDelete(*it, from, to, bDeleted, bTrimmed);

                    // Should seqloc be deleted from list?
                    if (bDeleted) {
                        it = mix->Set().erase(it);
                    }
                    else {
                        ++it;
                        bCompleteCut = false;
                    }
                }

                // Update the original list
                loc->SetMix(*mix);
            }
        }
        break;

        // Other choices not supported yet 
        default:
        {           
        }
        break;
    }
}


static void s_UpdateSeqGraphLoc(CRef<CSeq_graph> graph, 
                                const TCuts& sorted_cuts)
{
    for (TCuts::size_type ii = 0; ii < sorted_cuts.size(); ++ii) {
        const TRange& cut = sorted_cuts[ii];
        TSeqPos from = cut.GetFrom();
        TSeqPos to = cut.GetTo();

        if (graph->CanGetLoc()) {
            CRef<CSeq_graph::TLoc> new_loc(new CSeq_graph::TLoc);
            new_loc->Assign(graph->GetLoc());
            bool bDeleted = false;
            bool bTrimmed = false;
            s_SeqLocDelete(new_loc, from, to, bDeleted, bTrimmed);
            graph->SetLoc(*new_loc);
        }
    }
}


/// Trim Seq-graph annotation 
void TrimSeqGraph(CBioseq_Handle bsh, 
                  CRef<CSeq_graph> graph, 
                  const TCuts& sorted_cuts)
{
    // Get range that original seqgraph data covers
    TSeqPos graph_start = graph->GetLoc().GetStart(eExtreme_Positional);
    TSeqPos graph_stop = graph->GetLoc().GetStop(eExtreme_Positional);

    // Get range of trimmed sequence
    TSeqPos trim_start;
    TSeqPos trim_stop;
    s_GetTrimCoordinates(bsh, sorted_cuts, trim_start, trim_stop);

    // Determine range over which to copy seqgraph data from old to new
    TSeqPos copy_start = graph_start;
    if (trim_start > graph_start) {
        copy_start = trim_start;
    }
    TSeqPos copy_stop = graph_stop;
    if (trim_stop < graph_stop) {
        copy_stop = trim_stop;
    }

    // Copy over seqgraph data values.  Handle BYTE type only (see 
    // C Toolkit's GetGraphsProc function in api/sqnutil2.c)
    CSeq_graph::TGraph& dst_data = graph->SetGraph();
    switch ( dst_data.Which() ) {
    case CSeq_graph::TGraph::e_Byte:
        // Keep original min, max, axis

        // Copy start/stop values are relative to bioseq coordinate system.
        // Change them so that they are relative to the BYTE values container.
        copy_start -= graph_start;
        copy_stop -= graph_start;

        // Update data values via
        // 1) copy over the new range to another container
        // 2) swap
        CByte_graph::TValues subset;
        subset.assign(dst_data.GetByte().GetValues().begin() + copy_start,
                      dst_data.GetByte().GetValues().begin() + copy_stop + 1);
        dst_data.SetByte().SetValues().swap(subset);

        // Update numvals
        graph->SetNumval(copy_stop - copy_start + 1);

        // Update seqloc
        s_UpdateSeqGraphLoc(graph, sorted_cuts);
        break;
    }
}


bool s_FindSegment(const CDense_seg& denseg,
                   CDense_seg::TDim row,
                   TSeqPos pos,
                   CDense_seg::TNumseg& seg,
                   TSeqPos& seg_start)
{
    for (seg = 0; seg < denseg.GetNumseg(); ++seg) {
        TSignedSeqPos start = denseg.GetStarts()[seg * denseg.GetDim() + row];
        TSignedSeqPos len   = denseg.GetLens()[seg];
        if (start != -1) {
            if (pos >= start  &&  pos < start + len) {
                seg_start = start;
                return true;
            }
        }
    }
    return false;
}


void s_CutDensegSegment(CRef<CSeq_align> align, 
                        CDense_seg::TDim row,
                        TSeqPos pos)
{
    // Find the segment where pos occurs for the sequence (identified by 
    // row).
    // If pos is not the start of the segment, cut the segment in two, with 
    // one of the segments using pos as the new start.


    // Find the segment where pos lies
    const CDense_seg& denseg = align->GetSegs().GetDenseg();
    CDense_seg::TNumseg foundseg; 
    TSeqPos seg_start;
    if ( !s_FindSegment(denseg, row, pos, foundseg, seg_start) ) {
        return;
    }

    // Found our segment seg
    // If pos falls on segment boundary, do nothing
    if (pos == seg_start) {
        return;
    }


    // Cut the segment :
    // 1) Allocate a new denseg with numseg size = original size + 1
    // 2) Copy elements before the cut
    // 3) Split segment at pos
    // 4) Copy elements after the cut
    // 5) Replace old denseg with new denseg

    // Allocate a new denseg with numseg size = original size + 1
    CRef<CDense_seg> new_denseg(new CDense_seg);    
    new_denseg->SetDim( denseg.GetDim() );
    new_denseg->SetNumseg( denseg.GetNumseg() + 1 );
    ITERATE( CDense_seg::TIds, idI, denseg.GetIds() ) {
        CSeq_id *si = new CSeq_id;
        si->Assign(**idI);
        new_denseg->SetIds().push_back( CRef<CSeq_id>(si) );
    }

    // Copy elements (starts, lens, strands) before the cut (up to and including
    // foundseg-1 in original denseg)
    for (CDense_seg::TNumseg curseg = 0; curseg < foundseg; ++curseg) {
        // Copy starts
        for (CDense_seg::TDim curdim = 0; curdim < denseg.GetDim(); ++curdim) {
            TSeqPos index = curseg * denseg.GetDim() + curdim;
            new_denseg->SetStarts().push_back( denseg.GetStarts()[index] );
        }

        // Copy lens
        new_denseg->SetLens().push_back( denseg.GetLens()[curseg] );

        // Copy strands
        if ( denseg.IsSetStrands() ) {
            for (CDense_seg::TDim curdim = 0; curdim < denseg.GetDim(); 
                 ++curdim) 
            {
                TSeqPos index = curseg * denseg.GetDim() + curdim;
                new_denseg->SetStrands().push_back(denseg.GetStrands()[index]);
            }
        }
    }

    // Split segment at pos
    // First find the lengths of the split segments, first_len and second_len
    TSeqPos first_len, second_len;
    TSeqPos index = foundseg * denseg.GetDim() + row;
    if ( !denseg.IsSetStrands() || denseg.GetStrands()[index] != eNa_strand_minus )
    {
        first_len  = pos - seg_start;
        second_len = denseg.GetLens()[foundseg] - first_len;
    } 
    else {
        second_len = pos - seg_start;
        first_len  = denseg.GetLens()[foundseg] - second_len;
    }   

    // Set starts, strands, and lens for the split segments (foundseg and foundseg+1)
    // Populate foundseg in new denseg
    for (CDense_seg::TDim curdim = 0; curdim < denseg.GetDim(); ++curdim) {
        TSeqPos index = foundseg * denseg.GetDim() + curdim;
        if (denseg.GetStarts()[index] == -1) {
            new_denseg->SetStarts().push_back(-1);
        }
        else if (!denseg.IsSetStrands() || denseg.GetStrands()[index] != eNa_strand_minus) {
            new_denseg->SetStarts().push_back(denseg.GetStarts()[index]);
        }
        else {
            new_denseg->SetStarts().push_back(denseg.GetStarts()[index] + second_len);
        }

        if (denseg.IsSetStrands()) {
            new_denseg->SetStrands().push_back(denseg.GetStrands()[index]);
        }
    }    
    new_denseg->SetLens().push_back(first_len);
    // Populate foundseg+1 in new denseg
    for (CDense_seg::TDim curdim = 0; curdim < denseg.GetDim(); ++curdim) {
        TSeqPos index = foundseg * denseg.GetDim() + curdim;
        if (denseg.GetStarts()[index] == -1) {
            new_denseg->SetStarts().push_back(-1);
        }
        else if (!denseg.IsSetStrands() || denseg.GetStrands()[index] != eNa_strand_minus) {
            new_denseg->SetStarts().push_back(denseg.GetStarts()[index] + first_len);
        }
        else {
            new_denseg->SetStarts().push_back(denseg.GetStarts()[index]);
        }

        if (denseg.IsSetStrands()) {
            new_denseg->SetStrands().push_back(denseg.GetStrands()[index]);
        }
    }    
    new_denseg->SetLens().push_back(second_len);

    // Copy elements (starts, lens, strands) after the cut (starting from foundseg+1 in 
    // original denseg)
    for (CDense_seg::TNumseg curseg = foundseg+1; curseg < denseg.GetNumseg(); ++curseg) {
        // Copy starts
        for (CDense_seg::TDim curdim = 0; curdim < denseg.GetDim(); ++curdim) {
            TSeqPos index = curseg * denseg.GetDim() + curdim;
            new_denseg->SetStarts().push_back( denseg.GetStarts()[index] );
        }

        // Copy lens
        new_denseg->SetLens().push_back( denseg.GetLens()[curseg] );

        // Copy strands
        if ( denseg.IsSetStrands() ) {
            for (CDense_seg::TDim curdim = 0; curdim < denseg.GetDim(); 
                 ++curdim) 
            {
                TSeqPos index = curseg * denseg.GetDim() + curdim;
                new_denseg->SetStrands().push_back(denseg.GetStrands()[index]);
            }
        }
    }

    // Update 
    align->SetSegs().SetDenseg(*new_denseg);
}


/// Trim Seq-align annotation
void TrimSeqAlign(CBioseq_Handle bsh, 
                  CRef<CSeq_align> align, 
                  const TCuts& sorted_cuts)
{
    // Assumption:  only DENSEG type is supported so caller should
    // ensure only denseg alignments are passed in.
    const CDense_seg& denseg = align->GetSegs().GetDenseg();

    // On which "row" of the denseg does the bsh seqid lie?
    const CDense_seg::TIds& ids = denseg.GetIds();
    CDense_seg::TDim row = -1;
    for (CDense_seg::TIds::size_type rr = 0; rr < ids.size(); ++rr) {
        if (ids[rr]->Match( *(bsh.GetSeqId()) )) {
            row = rr;
            break;
        }
    }
    if ( row < 0 || !denseg.CanGetDim() || row >= denseg.GetDim() ) {
        return;
    }

    // Make the cuts
    for (TCuts::size_type ii = 0; ii < sorted_cuts.size(); ++ii) {
        const TRange& cut = sorted_cuts[ii];
        TSeqPos cut_from = cut.GetFrom();
        TSeqPos cut_to = cut.GetTo();

        TSeqPos cut_len = cut_to - cut_from + 1;
        if (cut_to < cut_from) {
            cut_len = cut_from - cut_to + 1;
            cut_from = cut_to;
        } 

        // Note: row is 0-based

        // May need to cut the segment at both start and stop positions
        // if they do not fall on segment boundaries
        s_CutDensegSegment(align, row, cut_from);
        s_CutDensegSegment(align, row, cut_from + cut_len);

        // Update segment start values for the trimmed sequence row
        const CDense_seg& denseg = align->GetSegs().GetDenseg();
        for (CDense_seg::TNumseg curseg = 0; curseg < denseg.GetNumseg(); ++curseg) {
            TSeqPos index = curseg * denseg.GetDim() + row;
            TSignedSeqPos seg_start = denseg.GetStarts()[index];
            if (seg_start < 0) {
                // This indicates a gap, no change needed
            }
            else if (seg_start < cut_from) {
                // This is before the cut, no change needed
            }
            else if (seg_start >= cut_from && 
                     seg_start + denseg.GetLens()[curseg] <= cut_from + cut_len) {
                // This is in the gap, indicate it with a -1
                align->SetSegs().SetDenseg().SetStarts()[index] = -1;
            }
            else {
                // This is after the cut - subtract the cut_len
                align->SetSegs().SetDenseg().SetStarts()[index] -= cut_len;
            }
        }
    }
}


/// Trim Seq-feat annotation
void TrimSeqFeat(CRef<CSeq_feat> feat, 
                 const TCuts& sorted_cuts,
                 bool& bFeatureDeleted, 
                 bool& bFeatureTrimmed)
{
    for (TCuts::size_type ii = 0; ii < sorted_cuts.size(); ++ii) {
        const TRange& cut = sorted_cuts[ii];
        TSeqPos from = cut.GetFrom();
        TSeqPos to = cut.GetTo();

        // Update Seqloc "feature made from" 
        if (feat->CanGetLocation()) {
            CRef<CSeq_feat::TLocation> new_location(new CSeq_feat::TLocation);
            new_location->Assign(feat->GetLocation());
            s_SeqLocDelete(new_location, from, to, bFeatureDeleted, bFeatureTrimmed);
            feat->SetLocation(*new_location);

            // No need to cut anymore nor update.  Feature will be completely
            // deleted.  
            if (bFeatureDeleted) {
                return;
            }
        }

        // Update Seqloc "product of process"
        if (feat->CanGetProduct()) {
            CRef<CSeq_feat::TProduct> new_product(new CSeq_feat::TProduct);
            new_product->Assign(feat->GetProduct());
            bool bProdDeleted = false;
            bool bProdTrimmed = false;
            s_SeqLocDelete(new_product, from, to, bProdDeleted, bProdTrimmed);
            feat->SetProduct(*new_product);
        }
    }
}


/// Secondary function needed after trimming Seq-feat.
/// If the trim completely covers the feature (boolean reference bFeatureDeleted
/// from TrimSeqFeat() returns true), then delete protein sequence and 
/// re-normalize nuc-prot set.
void DeleteProteinAndRenormalizeNucProtSet(const CSeq_feat_Handle& feat_h)
{
    // First, if the feature is a Cdregion, then delete the protein sequence
    CMappedFeat mapped_feat(feat_h);
    if ( mapped_feat.IsSetData() && 
         mapped_feat.GetData().Which() == CSeqFeatData::e_Cdregion &&
         mapped_feat.IsSetProduct() )
    {
        // Use Cdregion feat.product seqloc to get protein bioseq handle
        CBioseq_Handle prot_h = 
            mapped_feat.GetScope().GetBioseqHandle(mapped_feat.GetProduct());

        // Should be a protein!
        if ( prot_h.IsProtein() && !prot_h.IsRemoved() ) {
            // Get the protein parent set before you remove the protein
            CBioseq_set_Handle bssh = prot_h.GetParentBioseq_set();

            // Delete the protein
            CBioseq_EditHandle prot_eh(prot_h);
            prot_eh.Remove();

            // If lone nuc remains, renormalize the nuc-prot set
            if (bssh && bssh.IsSetClass() 
                && bssh.GetClass() == CBioseq_set::eClass_nuc_prot
                && !bssh.IsEmptySeq_set() 
                && bssh.GetBioseq_setCore()->GetSeq_set().size() == 1) 
            {
                // Renormalize the lone nuc that's inside the nuc-prot set into  
                // a nuc bioseq.  This call will remove annots/descrs from the 
                // set and attach them to the seq.
                bssh.GetParentEntry().GetEditHandle().ConvertSetToSeq();
            }
        }
    }

    // Finally, delete the feature
    CSeq_feat_EditHandle feat_eh(feat_h);
    feat_eh.Remove();
}


/// Secondary function needed after trimming Seq-feat.
/// If TrimSeqFeat()'s bFeatureTrimmed returns true, then adjust cdregion frame.
void AdjustCdregionFrame(TSeqPos original_nuc_len, 
                         CRef<CSeq_feat> cds,
                         const TCuts& sorted_cuts)
{
    // Get partialness and strand of location before cutting
    bool bIsPartialStart = false;
    ENa_strand eStrand = eNa_strand_unknown;
    if (cds->CanGetLocation()) {
        bIsPartialStart = cds->GetLocation().IsPartialStart(eExtreme_Biological);
        eStrand = cds->GetLocation().GetStrand(); 
    }

    for (TCuts::size_type ii = 0; ii < sorted_cuts.size(); ++ii) {
        const TRange& cut = sorted_cuts[ii];
        TSeqPos from = cut.GetFrom();
        TSeqPos to = cut.GetTo();

        // Adjust Seq-feat.data.cdregion frame
        if (cds->CanGetData() && 
            cds->GetData().GetSubtype() == CSeqFeatData::eSubtype_cdregion &&
            cds->GetData().IsCdregion())
        {
            // Make a copy
            CRef<CCdregion> new_cdregion(new CCdregion);
            new_cdregion->Assign(cds->GetData().GetCdregion());

            // Edit the copy
            if ( (eStrand == eNa_strand_minus &&
                  to == original_nuc_len - 1 &&
                  bIsPartialStart)
                 ||
                 (eStrand != eNa_strand_minus && 
                  from == 0 && 
                  bIsPartialStart) )
            {
                TSeqPos old_frame = new_cdregion->GetFrame();
                if (old_frame == 0) {
                    old_frame = 1;
                }

                TSignedSeqPos new_frame = old_frame - ((to - from + 1) % 3);
                if (new_frame < 1) {
                    new_frame += 3;
                }
                new_cdregion->SetFrame((CCdregion::EFrame)new_frame);
            }

            // Update the original
            cds->SetData().SetCdregion(*new_cdregion);
        }
    }
}


/// Secondary function needed after trimming Seq-feat.
/// If TrimSeqFeat()'s bFeatureTrimmed returns true, then make new protein
/// sequence.
CRef<CBioseq> SetNewProteinSequence(CScope& new_scope,
                                    CRef<CSeq_feat> cds,
                                    CRef<CSeq_inst> new_inst)
{
    CRef<CBioseq> new_protein_bioseq;
    if (new_inst->IsSetSeq_data()) {
        // Generate new protein sequence data and length
        new_protein_bioseq = CSeqTranslator::TranslateToProtein(*cds, new_scope);
        if (new_protein_bioseq->GetInst().GetSeq_data().IsIupacaa()) 
        {
            new_inst->SetSeq_data().SetIupacaa().Set( 
                new_protein_bioseq->GetInst().GetSeq_data().GetIupacaa().Get());
            new_inst->SetLength( new_protein_bioseq->GetInst().GetLength() );
        }
        else if (new_protein_bioseq->GetInst().GetSeq_data().IsNcbieaa()) 
        {
            new_inst->SetSeq_data().SetNcbieaa().Set( 
                new_protein_bioseq->GetInst().GetSeq_data().GetNcbieaa().Get());
            new_inst->SetLength( new_protein_bioseq->GetInst().GetLength() );
        }
    }
    return new_protein_bioseq;
}


/// Secondary function needed after trimming Seq-feat.
/// If TrimSeqFeat()'s bFeatureTrimmed returns true, then retranslate cdregion.
void RetranslateCdregion(CBioseq_Handle nuc_bsh,
                         CRef<CSeq_inst> trimmed_nuc_inst, 
                         CRef<CSeq_feat> cds,
                         const TCuts& sorted_cuts)
{
    if ( cds->IsSetData() && 
         cds->GetData().Which() == CSeqFeatData::e_Cdregion &&
         cds->IsSetProduct() )
    {
        // In order to retranslate correctly, we need to create a 
        // new scope with the trimmed sequence data.

        // Keep track of original seqinst
        CRef<objects::CSeq_inst> orig_inst(new objects::CSeq_inst());
        orig_inst->Assign(nuc_bsh.GetInst());

        // Update the seqinst to the trimmed version, set the scope
        // and retranslate
        CBioseq_EditHandle bseh = nuc_bsh.GetEditHandle();
        bseh.SetInst(*trimmed_nuc_inst);
        CScope& new_scope = bseh.GetScope();

        // Use Cdregion.Product to get handle to protein bioseq 
        CBioseq_Handle prot_bsh = new_scope.GetBioseqHandle(cds->GetProduct());
        if (!prot_bsh.IsProtein()) {
            return;
        }

        // Make a copy 
        CRef<CSeq_inst> new_inst(new CSeq_inst());
        new_inst->Assign(prot_bsh.GetInst());

        // Edit the copy
        CRef<CBioseq> new_protein_bioseq = 
            SetNewProteinSequence(new_scope, cds, new_inst);
        if ( !new_protein_bioseq ) {
            return;
        }

        // Update the original
        CBioseq_EditHandle prot_eh = prot_bsh.GetEditHandle();
        prot_eh.SetInst(*new_inst);

        // If protein feature exists, update it
        SAnnotSelector sel(CSeqFeatData::e_Prot);
        CFeat_CI prot_feat_ci(prot_bsh, sel);
        for ( ; prot_feat_ci; ++prot_feat_ci ) {
            // Make a copy
            CRef<CSeq_feat> new_feat(new CSeq_feat());
            new_feat->Assign(prot_feat_ci->GetOriginalFeature());

            if ( new_feat->CanGetLocation() &&
                 new_feat->GetLocation().IsInt() &&
                 new_feat->GetLocation().GetInt().CanGetTo() )
            {
                // Edit the copy
                new_feat->SetLocation().SetInt().SetTo(
                    new_protein_bioseq->GetLength() - 1);

                // Update the original
                CSeq_feat_EditHandle prot_feat_eh(*prot_feat_ci);
                prot_feat_eh.Replace(*new_feat);
            }
        }

        // Restore the original seqinst
        bseh.SetInst(*orig_inst);
    }
}

/*******************************************************************************
**** LOW-LEVEL API
****
**** Trim functions 
*******************************************************************************/


// For Unverified descriptors
CRef<CSeqdesc> FindUnverified(const CBioseq& seq)
{
    if (!seq.IsSetDescr()) {
        return CRef<CSeqdesc>(NULL);
    }
    ITERATE(CBioseq::TDescr::Tdata, it, seq.GetDescr().Get()) {
        if ((*it)->IsUser() && (*it)->GetUser().GetObjectType() == CUser_object::eObjectType_Unverified) {
            return *it;
        }
    }
    return CRef<CSeqdesc>(NULL);
}


bool IsUnverifiedOrganism(const CBioseq& seq)
{
    if (!seq.IsSetDescr()) {
        return false;
    }
    ITERATE(CBioseq::TDescr::Tdata, it, seq.GetDescr().Get()) {
        if ((*it)->IsUser() && (*it)->GetUser().IsUnverifiedOrganism()) {
            return true;
        }
    }
    return false;
}


bool IsUnverifiedFeature(const CBioseq& seq)
{
    if (!seq.IsSetDescr()) {
        return false;
    }
    ITERATE(CBioseq::TDescr::Tdata, it, seq.GetDescr().Get()) {
        if ((*it)->IsUser() && (*it)->GetUser().IsUnverifiedFeature()) {
            return true;
        }
    }
    return false;
}

void SortSeqDescr(CSeq_descr& descr)
{
    descr.Set().sort(CompareSeqdesc());
}

void SortSeqDescr(CSeq_entry& entry)
{
    if (entry.IsSetDescr())
      SortSeqDescr(entry.SetDescr());
    if (entry.IsSet())
    NON_CONST_ITERATE(CBioseq_set::TSeq_set, it, entry.SetSet().SetSeq_set())
    {
        SortSeqDescr((**it));
    }
}




END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
