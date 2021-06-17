#ifndef OBJECTS_SEQSET___SEQSET_MACROS__HPP
#define OBJECTS_SEQSET___SEQSET_MACROS__HPP

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
 * Authors:  Jonathan Kans, Michael Kornbluh, Colleen Bollin
 *
 */

/// @file seqset_macros.hpp
/// Utility macros and typedefs for exploring NCBI objects from seqset.asn.


#include <objects/misc/sequence_util_macros.hpp>
#include <objects/seqset/seqset__.hpp>


/// @NAME Convenience macros for NCBI objects
/// @{


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
/// Macros and typedefs for object subtypes
/////////////////////////////////////////////////////////////////////////////


/// CSeq_entry definitions

#define NCBI_SEQENTRY(Type) CSeq_entry::e_##Type
typedef CSeq_entry::E_Choice TSEQENTRY_CHOICE;

//   Seq     Set


/// CBioseq_set definitions

#define NCBI_BIOSEQSETCLASS(Type) CBioseq_set::eClass_##Type
typedef CBioseq_set::EClass TBIOSEQSETCLASS_TYPE;

// nuc_prot          segset            conset
// parts             gibb              gi
// genbank           pir               pub_set
// equiv             swissprot         pdb_entry
// mut_set           pop_set           phy_set
// eco_set           gen_prod_set      wgs_set
// named_annot       named_annot_prod  read_set
// paired_end_reads  small_genome_set  other



/////////////////////////////////////////////////////////////////////////////
/// Macros to recursively explore within NCBI data model objects
/////////////////////////////////////////////////////////////////////////////


/// VISIT_WITHIN_SEQENTRY base macro makes recursive iterator with generated components
/// VISIT_WITHIN_SEQSET base macro makes recursive iterator with generated components

#define VISIT_WITHIN_SEQENTRY(Typ, Itr, Var) \
NCBI_SERIAL_TEST_EXPLORE ((Var).Which() != CSeq_entry::e_not_set, Typ, Itr, (Var))

#define VISIT_WITHIN_SEQSET(Typ, Itr, Var) \
NCBI_SERIAL_TEST_EXPLORE ((Var).IsSetSeq_set(), Typ, Itr, (Var))

/// Base macros for editable exploration

#define EXPLORE_WITHIN_SEQENTRY(Typ, Itr, Var) \
NCBI_SERIAL_NC_EXPLORE ((Var).Which() != CSeq_entry::e_not_set, Typ, Itr, (Var))

#define EXPLORE_WITHIN_SEQSET(Typ, Itr, Var) \
NCBI_SERIAL_NC_EXPLORE ((Var).IsSetSeq_set(), Typ, Itr, (Var))


// "VISIT_ALL_XXX_WITHIN_YYY" does a recursive exploration of NCBI objects


/// CSeq_entry explorers

/// VISIT_ALL_SEQENTRYS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CSeq_entry& seqentry = *itr;

#define VISIT_ALL_SEQENTRYS_WITHIN_SEQENTRY(Itr, Var) \
VISIT_WITHIN_SEQENTRY (CSeq_entry, Itr, Var)

/// VISIT_ALL_BIOSEQS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CBioseq& bioseq = *itr;

#define VISIT_ALL_BIOSEQS_WITHIN_SEQENTRY(Itr, Var) \
VISIT_WITHIN_SEQENTRY (CBioseq, Itr, Var)

/// EXPLORE_ALL_BIOSEQS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with CBioseq& bioseq = *itr;

#define EXPLORE_ALL_BIOSEQS_WITHIN_SEQENTRY(Itr, Var) \
EXPLORE_WITHIN_SEQENTRY (CBioseq, Itr, Var)

/// VISIT_ALL_SEQSETS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CBioseq_set& bss = *itr;

#define VISIT_ALL_SEQSETS_WITHIN_SEQENTRY(Itr, Var) \
VISIT_WITHIN_SEQENTRY (CBioseq_set, Itr, Var)

/// VISIT_ALL_SEQDESCS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CSeqdesc& desc = *itr;

#define VISIT_ALL_SEQDESCS_WITHIN_SEQENTRY(Itr, Var) \
VISIT_WITHIN_SEQENTRY (CSeqdesc, Itr, Var)

#define VISIT_ALL_DESCRIPTORS_WITHIN_SEQENTRY VISIT_ALL_SEQDESCS_WITHIN_SEQENTRY

/// VISIT_ALL_SEQANNOTS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CSeq_annot& annot = *itr;

#define VISIT_ALL_SEQANNOTS_WITHIN_SEQENTRY(Itr, Var) \
VISIT_WITHIN_SEQENTRY (CSeq_annot, Itr, Var)

#define VISIT_ALL_ANNOTS_WITHIN_SEQENTRY VISIT_ALL_SEQANNOTS_WITHIN_SEQENTRY

/// VISIT_ALL_SEQFEATS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CSeq_feat& feat = *itr;

#define VISIT_ALL_SEQFEATS_WITHIN_SEQENTRY(Itr, Var) \
VISIT_WITHIN_SEQENTRY (CSeq_feat, Itr, Var)

#define VISIT_ALL_FEATURES_WITHIN_SEQENTRY VISIT_ALL_SEQFEATS_WITHIN_SEQENTRY

/// VISIT_ALL_SEQALIGNS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CSeq_align& align = *itr;

#define VISIT_ALL_SEQALIGNS_WITHIN_SEQENTRY(Itr, Var) \
VISIT_WITHIN_SEQENTRY (CSeq_align, Itr, Var)

#define VISIT_ALL_ALIGNS_WITHIN_SEQENTRY VISIT_ALL_SEQALIGNS_WITHIN_SEQENTRY

/// VISIT_ALL_SEQGRAPHS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CSeq_graph& graph = *itr;

#define VISIT_ALL_SEQGRAPHS_WITHIN_SEQENTRY(Itr, Var) \
VISIT_WITHIN_SEQENTRY (CSeq_graph, Itr, Var)

#define VISIT_ALL_GRAPHS_WITHIN_SEQENTRY VISIT_ALL_SEQGRAPHS_WITHIN_SEQENTRY


/// CBioseq_set explorers

/// VISIT_ALL_SEQENTRYS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CSeq_entry& seqentry = *itr;

#define VISIT_ALL_SEQENTRYS_WITHIN_SEQSET(Itr, Var) \
VISIT_WITHIN_SEQSET (CSeq_entry, Itr, Var)

/// VISIT_ALL_BIOSEQS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CBioseq& bioseq = *itr;

#define VISIT_ALL_BIOSEQS_WITHIN_SEQSET(Itr, Var) \
VISIT_WITHIN_SEQSET (CBioseq, Itr, Var)

/// EXPLORE_ALL_BIOSEQS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with CBioseq& bioseq = *itr;

#define EXPLORE_ALL_BIOSEQS_WITHIN_SEQSET(Itr, Var) \
EXPLORE_WITHIN_SEQSET (CBioseq, Itr, Var)

/// VISIT_ALL_SEQSETS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CBioseq_set& bss = *itr;

#define VISIT_ALL_SEQSETS_WITHIN_SEQSET(Itr, Var) \
VISIT_WITHIN_SEQSET (CBioseq_set, Itr, Var)

/// VISIT_ALL_SEQDESCS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CSeqdesc& desc = *itr;

#define VISIT_ALL_SEQDESCS_WITHIN_SEQSET(Itr, Var) \
VISIT_WITHIN_SEQSET (CSeqdesc, Itr, Var)

#define VISIT_ALL_DESCRIPTORS_WITHIN_SEQSET VISIT_ALL_SEQDESCS_WITHIN_SEQSET

/// VISIT_ALL_SEQANNOTS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CSeq_annot& annot = *itr;

#define VISIT_ALL_SEQANNOTS_WITHIN_SEQSET(Itr, Var) \
VISIT_WITHIN_SEQSET (CSeq_annot, Itr, Var)

#define VISIT_ALL_ANNOTS_WITHIN_SEQSET VISIT_ALL_SEQANNOTS_WITHIN_SEQSET

/// VISIT_ALL_SEQFEATS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CSeq_feat& feat = *itr;

#define VISIT_ALL_SEQFEATS_WITHIN_SEQSET(Itr, Var) \
VISIT_WITHIN_SEQSET (CSeq_feat, Itr, Var)

#define VISIT_ALL_FEATURES_WITHIN_SEQSET VISIT_ALL_SEQFEATS_WITHIN_SEQSET

/// VISIT_ALL_SEQALIGNS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CSeq_align& align = *itr;

#define VISIT_ALL_SEQALIGNS_WITHIN_SEQSET(Itr, Var) \
VISIT_WITHIN_SEQSET (CSeq_align, Itr, Var)

#define VISIT_ALL_ALIGNS_WITHIN_SEQSET VISIT_ALL_SEQALIGNS_WITHIN_SEQSET

/// VISIT_ALL_SEQGRAPHS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CSeq_graph& graph = *itr;

#define VISIT_ALL_SEQGRAPHS_WITHIN_SEQSET(Itr, Var) \
VISIT_WITHIN_SEQSET (CSeq_graph, Itr, Var)

#define VISIT_ALL_GRAPHS_WITHIN_SEQSET VISIT_ALL_SEQGRAPHS_WITHIN_SEQSET


// "FOR_EACH_XXX_ON_YYY" does a linear const traversal of STL containers
// "EDIT_EACH_XXX_ON_YYY" does a linear non-const traversal of STL containers

// "SWITCH_ON_XXX_CHOICE" switches on the item subtype

// "ADD_XXX_TO_YYY" adds an element to a specified object
// "ERASE_XXX_ON_YYY" deletes a specified object within an iterator

// Miscellaneous macros for testing objects include
// "XXX_IS_YYY" or "XXX_HAS_YYY"
// "XXX_CHOICE_IS"


///
/// CSeq_entry macros

/// SEQENTRY_CHOICE macros

#define SEQENTRY_CHOICE_Test(Var) (Var).Which() != CSeq_entry::e_not_set
#define SEQENTRY_CHOICE_Chs(Var)  (Var).Which()

/// SEQENTRY_CHOICE_IS

#define SEQENTRY_CHOICE_IS(Var, Chs) \
CHOICE_IS (SEQENTRY_CHOICE, Var, Chs)

/// SEQENTRY_IS_SEQ

#define SEQENTRY_IS_SEQ(Var) \
SEQENTRY_CHOICE_IS (Var, NCBI_SEQENTRY(Seq))

/// SEQENTRY_IS_SET

#define SEQENTRY_IS_SET(Var) \
SEQENTRY_CHOICE_IS (Var, NCBI_SEQENTRY(Set))

/// SWITCH_ON_SEQENTRY_CHOICE

#define SWITCH_ON_SEQENTRY_CHOICE(Var) \
SWITCH_ON (SEQENTRY_CHOICE, Var)


/// SEQDESC_ON_SEQENTRY macros

#define SEQDESC_ON_SEQENTRY_Type      CSeq_descr::Tdata
#define SEQDESC_ON_SEQENTRY_Test(Var) (Var).IsSetDescr()
#define SEQDESC_ON_SEQENTRY_Get(Var)  (Var).GetDescr().Get()
#define SEQDESC_ON_SEQENTRY_Set(Var)  (Var).SetDescr().Set()

/// SEQENTRY_HAS_SEQDESC

#define SEQENTRY_HAS_SEQDESC(Var) \
ITEM_HAS (SEQDESC_ON_SEQENTRY, Var)

/// FOR_EACH_SEQDESC_ON_SEQENTRY
/// EDIT_EACH_SEQDESC_ON_SEQENTRY
// CSeq_entry& as input, dereference with [const] CSeqdesc& desc = **itr

#define FOR_EACH_SEQDESC_ON_SEQENTRY(Itr, Var) \
FOR_EACH (SEQDESC_ON_SEQENTRY, Itr, Var)

#define EDIT_EACH_SEQDESC_ON_SEQENTRY(Itr, Var) \
EDIT_EACH (SEQDESC_ON_SEQENTRY, Itr, Var)

/// ADD_SEQDESC_TO_SEQENTRY

#define ADD_SEQDESC_TO_SEQENTRY(Var, Ref) \
ADD_ITEM (SEQDESC_ON_SEQENTRY, Var, Ref)

/// ERASE_SEQDESC_ON_SEQENTRY

#define ERASE_SEQDESC_ON_SEQENTRY(Itr, Var) \
LIST_ERASE_ITEM (SEQDESC_ON_SEQENTRY, Itr, Var)

/// SEQDESC_ON_SEQENTRY_IS_SORTED

#define SEQDESC_ON_SEQENTRY_IS_SORTED( Var, Func ) \
    IS_SORTED (SEQDESC_ON_SEQENTRY, Var, Func)

#define SORT_SEQDESC_ON_SEQENTRY(Var, Func) \
    DO_LIST_SORT (SEQDESC_ON_SEQENTRY, Var, Func)

/// SEQENTRY_HAS_DESCRIPTOR
/// FOR_EACH_DESCRIPTOR_ON_SEQENTRY
/// EDIT_EACH_DESCRIPTOR_ON_SEQENTRY
/// ADD_DESCRIPTOR_TO_SEQENTRY
/// ERASE_DESCRIPTOR_ON_SEQENTRY

#define SEQENTRY_HAS_DESCRIPTOR SEQENTRY_HAS_SEQDESC
#define FOR_EACH_DESCRIPTOR_ON_SEQENTRY FOR_EACH_SEQDESC_ON_SEQENTRY
#define EDIT_EACH_DESCRIPTOR_ON_SEQENTRY EDIT_EACH_SEQDESC_ON_SEQENTRY
#define ADD_DESCRIPTOR_TO_SEQENTRY ADD_SEQDESC_TO_SEQENTRY
#define ERASE_DESCRIPTOR_ON_SEQENTRY ERASE_SEQDESC_ON_SEQENTRY


/// SEQANNOT_ON_SEQENTRY macros

#define SEQANNOT_ON_SEQENTRY_Type      CSeq_entry::TAnnot
#define SEQANNOT_ON_SEQENTRY_Test(Var) (Var).IsSetAnnot()
#define SEQANNOT_ON_SEQENTRY_Get(Var)  (Var).GetAnnot()
#define SEQANNOT_ON_SEQENTRY_Set(Var)  (Var).(Seq).SetAnnot()

/// SEQENTRY_HAS_SEQANNOT

#define SEQENTRY_HAS_SEQANNOT(Var) \
ITEM_HAS (SEQANNOT_ON_SEQENTRY, Var)

/// FOR_EACH_SEQANNOT_ON_SEQENTRY
/// EDIT_EACH_SEQANNOT_ON_SEQENTRY
// CSeq_entry& as input, dereference with [const] CSeq_annot& annot = **itr;

#define FOR_EACH_SEQANNOT_ON_SEQENTRY(Itr, Var) \
FOR_EACH (SEQANNOT_ON_SEQENTRY, Itr, Var)

#define EDIT_EACH_SEQANNOT_ON_SEQENTRY(Itr, Var) \
EDIT_EACH (SEQANNOT_ON_SEQENTRY, Itr, Var)

/// ADD_SEQANNOT_TO_SEQENTRY

#define ADD_SEQANNOT_TO_SEQENTRY(Var, Ref) \
ADD_ITEM (SEQANNOT_ON_SEQENTRY, Var, Ref)

/// ERASE_SEQANNOT_ON_SEQENTRY

#define ERASE_SEQANNOT_ON_SEQENTRY(Itr, Var) \
LIST_ERASE_ITEM (SEQANNOT_ON_SEQENTRY, Itr, Var)

/// SEQENTRY_HAS_ANNOT
/// FOR_EACH_ANNOT_ON_SEQENTRY
/// EDIT_EACH_ANNOT_ON_SEQENTRY
/// ADD_ANNOT_TO_SEQENTRY
/// ERASE_ANNOT_ON_SEQENTRY

#define SEQENTRY_HAS_ANNOT SEQENTRY_HAS_SEQANNOT
#define FOR_EACH_ANNOT_ON_SEQENTRY FOR_EACH_SEQANNOT_ON_SEQENTRY
#define EDIT_EACH_ANNOT_ON_SEQENTRY EDIT_EACH_SEQANNOT_ON_SEQENTRY
#define ADD_ANNOT_TO_SEQENTRY ADD_SEQANNOT_TO_SEQENTRY
#define ERASE_ANNOT_ON_SEQENTRY ERASE_SEQANNOT_ON_SEQENTRY


///
/// CBioseq_set macros

/// SEQDESC_ON_SEQSET macros

#define SEQDESC_ON_SEQSET_Type      CBioseq_set::TDescr::Tdata
#define SEQDESC_ON_SEQSET_Test(Var) (Var).IsSetDescr()
#define SEQDESC_ON_SEQSET_Get(Var)  (Var).GetDescr().Get()
#define SEQDESC_ON_SEQSET_Set(Var)  (Var).SetDescr().Set()

/// SEQSET_HAS_SEQDESC

#define SEQSET_HAS_SEQDESC(Var) \
ITEM_HAS (SEQDESC_ON_SEQSET, Var)

/// FOR_EACH_SEQDESC_ON_SEQSET
/// EDIT_EACH_SEQDESC_ON_SEQSET
// CBioseq_set& as input, dereference with [const] CSeqdesc& desc = **itr;

#define FOR_EACH_SEQDESC_ON_SEQSET(Itr, Var) \
FOR_EACH (SEQDESC_ON_SEQSET, Itr, Var)

#define EDIT_EACH_SEQDESC_ON_SEQSET(Itr, Var) \
EDIT_EACH (SEQDESC_ON_SEQSET, Itr, Var)

/// ADD_SEQDESC_TO_SEQSET

#define ADD_SEQDESC_TO_SEQSET(Var, Ref) \
ADD_ITEM (SEQDESC_ON_SEQSET, Var, Ref)

/// ERASE_SEQDESC_ON_SEQSET

#define ERASE_SEQDESC_ON_SEQSET(Itr, Var) \
LIST_ERASE_ITEM (SEQDESC_ON_SEQSET, Itr, Var)

/// SEQSET_HAS_DESCRIPTOR
/// FOR_EACH_DESCRIPTOR_ON_SEQSET
/// EDIT_EACH_DESCRIPTOR_ON_SEQSET
/// ADD_DESCRIPTOR_TO_SEQSET
/// ERASE_DESCRIPTOR_ON_SEQSET

#define SEQSET_HAS_DESCRIPTOR SEQSET_HAS_SEQDESC
#define FOR_EACH_DESCRIPTOR_ON_SEQSET FOR_EACH_SEQDESC_ON_SEQSET
#define EDIT_EACH_DESCRIPTOR_ON_SEQSET EDIT_EACH_SEQDESC_ON_SEQSET
#define ADD_DESCRIPTOR_TO_SEQSET ADD_SEQDESC_TO_SEQSET
#define ERASE_DESCRIPTOR_ON_SEQSET ERASE_SEQDESC_ON_SEQSET


/// SEQANNOT_ON_SEQSET macros

#define SEQANNOT_ON_SEQSET_Type      CBioseq_set::TAnnot
#define SEQANNOT_ON_SEQSET_Test(Var) (Var).IsSetAnnot()
#define SEQANNOT_ON_SEQSET_Get(Var)  (Var).GetAnnot()
#define SEQANNOT_ON_SEQSET_Set(Var)  (Var).SetAnnot()

/// SEQSET_HAS_SEQANNOT

#define SEQSET_HAS_SEQANNOT(Var) \
ITEM_HAS (SEQANNOT_ON_SEQSET, Var)

/// FOR_EACH_SEQANNOT_ON_SEQSET
/// EDIT_EACH_SEQANNOT_ON_SEQSET
// CBioseq_set& as input, dereference with [const] CSeq_annot& annot = **itr;

#define FOR_EACH_SEQANNOT_ON_SEQSET(Itr, Var) \
FOR_EACH (SEQANNOT_ON_SEQSET, Itr, Var)

#define EDIT_EACH_SEQANNOT_ON_SEQSET(Itr, Var) \
EDIT_EACH (SEQANNOT_ON_SEQSET, Itr, Var)

/// ADD_SEQANNOT_TO_SEQSET

#define ADD_SEQANNOT_TO_SEQSET(Var, Ref) \
ADD_ITEM (SEQANNOT_ON_SEQSET, Var, Ref)

/// ERASE_SEQANNOT_ON_SEQSET

#define ERASE_SEQANNOT_ON_SEQSET(Itr, Var) \
LIST_ERASE_ITEM (SEQANNOT_ON_SEQSET, Itr, Var)

/// SEQSET_HAS_ANNOT
/// FOR_EACH_ANNOT_ON_SEQSET
/// EDIT_EACH_ANNOT_ON_SEQSET
/// ADD_ANNOT_TO_SEQSET
/// ERASE_ANNOT_ON_SEQSET

#define SEQSET_HAS_ANNOT SEQSET_HAS_SEQANNOT
#define FOR_EACH_ANNOT_ON_SEQSET FOR_EACH_SEQANNOT_ON_SEQSET
#define EDIT_EACH_ANNOT_ON_SEQSET EDIT_EACH_SEQANNOT_ON_SEQSET
#define ADD_ANNOT_TO_SEQSET ADD_SEQANNOT_TO_SEQSET
#define ERASE_ANNOT_ON_SEQSET ERASE_SEQANNOT_ON_SEQSET


/// SEQENTRY_ON_SEQSET macros

#define SEQENTRY_ON_SEQSET_Type      CBioseq_set::TSeq_set
#define SEQENTRY_ON_SEQSET_Test(Var) (Var).IsSetSeq_set()
#define SEQENTRY_ON_SEQSET_Get(Var)  (Var).GetSeq_set()
#define SEQENTRY_ON_SEQSET_Set(Var)  (Var).SetSeq_set()

/// SEQSET_HAS_SEQENTRY

#define SEQSET_HAS_SEQENTRY(Var) \
ITEM_HAS (SEQENTRY_ON_SEQSET, Var)

/// FOR_EACH_SEQENTRY_ON_SEQSET
/// EDIT_EACH_SEQENTRY_ON_SEQSET
// CBioseq_set& as input, dereference with [const] CSeq_entry& se = **itr;

#define FOR_EACH_SEQENTRY_ON_SEQSET(Itr, Var) \
FOR_EACH (SEQENTRY_ON_SEQSET, Itr, Var)

#define EDIT_EACH_SEQENTRY_ON_SEQSET(Itr, Var) \
EDIT_EACH (SEQENTRY_ON_SEQSET, Itr, Var)

/// ADD_SEQENTRY_TO_SEQSET

#define ADD_SEQENTRY_TO_SEQSET(Var, Ref) \
ADD_ITEM (SEQENTRY_ON_SEQSET, Var, Ref)

/// ERASE_SEQENTRY_ON_SEQSET

#define ERASE_SEQENTRY_ON_SEQSET(Itr, Var) \
LIST_ERASE_ITEM (SEQENTRY_ON_SEQSET, Itr, Var)


END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */

#endif  /* OBJECTS_SEQSET___SEQSET_MACROS__HPP */
