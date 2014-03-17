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
#include <objects/general/User_object.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_descr_ci.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/bioseq_set_handle.hpp>
#include <objtools/edit/edit_exception.hpp>
#include <objtools/edit/seq_entry_edit.hpp>
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
}

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


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
