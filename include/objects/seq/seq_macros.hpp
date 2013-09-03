#ifndef OBJECTS_SEQ___SEQ_MACROS__HPP
#define OBJECTS_SEQ___SEQ_MACROS__HPP

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

/// @file seq_macros.hpp
/// Utility macros and typedefs for exploring NCBI objects from seq.asn.


#include <objects/misc/sequence_util_macros.hpp>
#include <objects/seq/seq__.hpp>


/// @NAME Convenience macros for NCBI objects
/// @{


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
/// Macros and typedefs for object subtypes
/////////////////////////////////////////////////////////////////////////////


/// CSeq_inst definitions

#define NCBI_SEQREPR(Type) CSeq_inst::eRepr_##Type
typedef CSeq_inst::TRepr TSEQ_REPR;

//   virtual     raw     seg       const     ref
//   consen      map     delta     other

#define NCBI_SEQMOL(Type) CSeq_inst::eMol_##Type
typedef CSeq_inst::TMol TSEQ_MOL;

//   dna     rna     aa     na     other

#define NCBI_SEQTOPOLOGY(Type) CSeq_inst::eTopology_##Type
typedef CSeq_inst::TTopology TSEQ_TOPOLOGY;

//   linear     circular     tandem     other

#define NCBI_SEQSTRAND(Type) CSeq_inst::eStrand_##Type
typedef CSeq_inst::TStrand TSEQ_STRAND;

//   ss     ds     mixed     other


/// CSeq_annot definitions

#define NCBI_SEQANNOT(Type) CSeq_annot::TData::e_##Type
typedef CSeq_annot::TData::E_Choice TSEQANNOT_CHOICE;

//   Ftable     Align     Graph     Ids     Locs     Seq_table


/// CAnnotdesc definitions

#define NCBI_ANNOTDESC(Type) CAnnotdesc::e_##Type
typedef CAnnotdesc::E_Choice TANNOTDESC_CHOICE;

//   Name     Title           Comment         Pub
//   User     Create_date     Update_date
//   Src      Align           Region


/// CSeqdesc definitions

#define NCBI_SEQDESC(Type) CSeqdesc::e_##Type
typedef CSeqdesc::E_Choice TSEQDESC_CHOICE;

//   Mol_type     Modif           Method          Name
//   Title        Org             Comment         Num
//   Maploc       Pir             Genbank         Pub
//   Region       User            Sp              Dbxref
//   Embl         Create_date     Update_date     Prf
//   Pdb          Het             Source          Molinfo


/// CMolInfo definitions

#define NCBI_BIOMOL(Type) CMolInfo::eBiomol_##Type
typedef CMolInfo::TBiomol TMOLINFO_BIOMOL;

//   genomic             pre_RNA          mRNA      rRNA
//   tRNA                snRNA            scRNA     peptide
//   other_genetic       genomic_mRNA     cRNA      snoRNA
//   transcribed_RNA     ncRNA            tmRNA     other

#define NCBI_TECH(Type) CMolInfo::eTech_##Type
typedef CMolInfo::TTech TMOLINFO_TECH;

//   standard               est                  sts
//   survey                 genemap              physmap
//   derived                concept_trans        seq_pept
//   both                   seq_pept_overlap     seq_pept_homol
//   concept_trans_a        htgs_1               htgs_2
//   htgs_3                 fli_cdna             htgs_0
//   htc                    wgs                  barcode
//   composite_wgs_htgs     tsa                  other

#define NCBI_COMPLETENESS(Type) CMolInfo::eCompleteness_##Type
typedef CMolInfo::TCompleteness TMOLINFO_COMPLETENESS;

//   complete     partial      no_left       no_right
//   no_ends      has_left     has_right     other



/////////////////////////////////////////////////////////////////////////////
/// Macros for obtaining closest specific CSeqdesc applying to a CBioseq
/////////////////////////////////////////////////////////////////////////////


/// IF_EXISTS_CLOSEST base macro calls GetClosestDescriptor with generated components
// If Lvl is not NULL, it must be a pointer to an int

#define IF_EXISTS_CLOSEST(Cref, Var, Lvl, Chs) \
if (CConstRef<CSeqdesc> Cref = (Var).GetClosestDescriptor (Chs, Lvl))


/// IF_EXISTS_CLOSEST_MOLINFO
// CBioseq& as input, dereference with const CMolInfo& molinf = (*cref).GetMolinfo();

#define IF_EXISTS_CLOSEST_MOLINFO(Cref, Var, Lvl) \
IF_EXISTS_CLOSEST (Cref, Var, Lvl, NCBI_SEQDESC(Molinfo))

/// IF_EXISTS_CLOSEST_BIOSOURCE
// CBioseq& as input, dereference with const CBioSource& source = (*cref).GetSource();

#define IF_EXISTS_CLOSEST_BIOSOURCE(Cref, Var, Lvl) \
IF_EXISTS_CLOSEST (Cref, Var, Lvl, NCBI_SEQDESC(Source))

/// IF_EXISTS_CLOSEST_TITLE
// CBioseq& as input, dereference with const string& title = (*cref).GetTitle();

#define IF_EXISTS_CLOSEST_TITLE(Cref, Var, Lvl) \
IF_EXISTS_CLOSEST (Cref, Var, Lvl, NCBI_SEQDESC(Title))

/// IF_EXISTS_CLOSEST_GENBANKBLOCK
// CBioseq& as input, dereference with const CGB_block& gbk = (*cref).GetGenbank();

#define IF_EXISTS_CLOSEST_GENBANKBLOCK(Cref, Var, Lvl) \
IF_EXISTS_CLOSEST (Cref, Var, Lvl, NCBI_SEQDESC(Genbank))

/// IF_EXISTS_CLOSEST_EMBLBLOCK
// CBioseq& as input, dereference with const CEMBL_block& ebk = (*cref).GetEmbl();

#define IF_EXISTS_CLOSEST_EMBLBLOCK(Cref, Var, Lvl) \
IF_EXISTS_CLOSEST (Cref, Var, Lvl, NCBI_SEQDESC(Embl))

/// IF_EXISTS_CLOSEST_PDBBLOCK
// CBioseq& as input, dereference with const CPDB_block& pbk = (*cref).GetPdb();

#define IF_EXISTS_CLOSEST_PDBBLOCK(Cref, Var, Lvl) \
IF_EXISTS_CLOSEST (Cref, Var, Lvl, NCBI_SEQDESC(Pdb))



// "FOR_EACH_XXX_ON_YYY" does a linear const traversal of STL containers
// "EDIT_EACH_XXX_ON_YYY" does a linear non-const traversal of STL containers

// "SWITCH_ON_XXX_CHOICE" switches on the item subtype

// "ADD_XXX_TO_YYY" adds an element to a specified object
// "ERASE_XXX_ON_YYY" deletes a specified object within an iterator

// Miscellaneous macros for testing objects include
// "XXX_IS_YYY" or "XXX_HAS_YYY"
// "XXX_CHOICE_IS"
///
/// CBioseq macros

/// SEQDESC_ON_BIOSEQ macros

#define SEQDESC_ON_BIOSEQ_Type      CBioseq::TDescr::Tdata
#define SEQDESC_ON_BIOSEQ_Test(Var) (Var).IsSetDescr()
#define SEQDESC_ON_BIOSEQ_Get(Var)  (Var).GetDescr().Get()
#define SEQDESC_ON_BIOSEQ_Set(Var)  (Var).SetDescr().Set()

/// BIOSEQ_HAS_SEQDESC

#define BIOSEQ_HAS_SEQDESC(Var) \
ITEM_HAS (SEQDESC_ON_BIOSEQ, Var)

/// FOR_EACH_SEQDESC_ON_BIOSEQ
/// EDIT_EACH_SEQDESC_ON_BIOSEQ
// CBioseq& as input, dereference with [const] CSeqdesc& desc = **itr

#define FOR_EACH_SEQDESC_ON_BIOSEQ(Itr, Var) \
FOR_EACH (SEQDESC_ON_BIOSEQ, Itr, Var)

#define EDIT_EACH_SEQDESC_ON_BIOSEQ(Itr, Var) \
EDIT_EACH (SEQDESC_ON_BIOSEQ, Itr, Var)

/// ADD_SEQDESC_TO_BIOSEQ

#define ADD_SEQDESC_TO_BIOSEQ(Var, Ref) \
ADD_ITEM (SEQDESC_ON_BIOSEQ, Var, Ref)

/// ERASE_SEQDESC_ON_BIOSEQ

#define ERASE_SEQDESC_ON_BIOSEQ(Itr, Var) \
LIST_ERASE_ITEM (SEQDESC_ON_BIOSEQ, Itr, Var)

/// BIOSEQ_HAS_DESCRIPTOR
/// FOR_EACH_DESCRIPTOR_ON_BIOSEQ
/// EDIT_EACH_DESCRIPTOR_ON_BIOSEQ
/// ADD_DESCRIPTOR_TO_BIOSEQ
/// ERASE_DESCRIPTOR_ON_BIOSEQ

#define BIOSEQ_HAS_DESCRIPTOR BIOSEQ_HAS_SEQDESC
#define FOR_EACH_DESCRIPTOR_ON_BIOSEQ FOR_EACH_SEQDESC_ON_BIOSEQ
#define EDIT_EACH_DESCRIPTOR_ON_BIOSEQ EDIT_EACH_SEQDESC_ON_BIOSEQ
#define ADD_DESCRIPTOR_TO_BIOSEQ ADD_SEQDESC_TO_BIOSEQ
#define ERASE_DESCRIPTOR_ON_BIOSEQ ERASE_SEQDESC_ON_BIOSEQ


/// SEQANNOT_ON_BIOSEQ macros

#define SEQANNOT_ON_BIOSEQ_Type      CBioseq::TAnnot
#define SEQANNOT_ON_BIOSEQ_Test(Var) (Var).IsSetAnnot()
#define SEQANNOT_ON_BIOSEQ_Get(Var)  (Var).GetAnnot()
#define SEQANNOT_ON_BIOSEQ_Set(Var)  (Var).SetAnnot()

/// BIOSEQ_HAS_SEQANNOT

#define BIOSEQ_HAS_SEQANNOT(Var) \
ITEM_HAS (SEQANNOT_ON_BIOSEQ, Var)

/// FOR_EACH_SEQANNOT_ON_BIOSEQ
/// EDIT_EACH_SEQANNOT_ON_BIOSEQ
// CBioseq& as input, dereference with [const] CSeq_annot& annot = **itr;

#define FOR_EACH_SEQANNOT_ON_BIOSEQ(Itr, Var) \
FOR_EACH (SEQANNOT_ON_BIOSEQ, Itr, Var)

#define EDIT_EACH_SEQANNOT_ON_BIOSEQ(Itr, Var) \
EDIT_EACH (SEQANNOT_ON_BIOSEQ, Itr, Var)

/// ADD_SEQANNOT_TO_BIOSEQ

#define ADD_SEQANNOT_TO_BIOSEQ(Var, Ref) \
ADD_ITEM (SEQANNOT_ON_BIOSEQ, Var, Ref)

/// ERASE_SEQANNOT_ON_BIOSEQ

#define ERASE_SEQANNOT_ON_BIOSEQ(Itr, Var) \
LIST_ERASE_ITEM (SEQANNOT_ON_BIOSEQ, Itr, Var)

/// BIOSEQ_HAS_ANNOT
/// FOR_EACH_ANNOT_ON_BIOSEQ
/// EDIT_EACH_ANNOT_ON_BIOSEQ
/// ADD_ANNOT_TO_BIOSEQ
/// ERASE_ANNOT_ON_BIOSEQ

#define BIOSEQ_HAS_ANNOT BIOSEQ_HAS_SEQANNOT
#define FOR_EACH_ANNOT_ON_BIOSEQ FOR_EACH_SEQANNOT_ON_BIOSEQ
#define EDIT_EACH_ANNOT_ON_BIOSEQ EDIT_EACH_SEQANNOT_ON_BIOSEQ
#define ADD_ANNOT_TO_BIOSEQ ADD_SEQANNOT_TO_BIOSEQ
#define ERASE_ANNOT_ON_BIOSEQ ERASE_SEQANNOT_ON_BIOSEQ


/// SEQID_ON_BIOSEQ macros

#define SEQID_ON_BIOSEQ_Type      CBioseq::TId
#define SEQID_ON_BIOSEQ_Test(Var) (Var).IsSetId()
#define SEQID_ON_BIOSEQ_Get(Var)  (Var).GetId()
#define SEQID_ON_BIOSEQ_Set(Var)  (Var).SetId()

/// BIOSEQ_HAS_SEQID

#define BIOSEQ_HAS_SEQID(Var) \
ITEM_HAS (SEQID_ON_BIOSEQ, Var)

/// FOR_EACH_SEQID_ON_BIOSEQ
/// EDIT_EACH_SEQID_ON_BIOSEQ
// CBioseq& as input, dereference with [const] CSeq_id& sid = **itr;

#define FOR_EACH_SEQID_ON_BIOSEQ(Itr, Var) \
FOR_EACH (SEQID_ON_BIOSEQ, Itr, Var)

#define EDIT_EACH_SEQID_ON_BIOSEQ(Itr, Var) \
EDIT_EACH (SEQID_ON_BIOSEQ, Itr, Var)

/// ADD_SEQID_TO_BIOSEQ

#define ADD_SEQID_TO_BIOSEQ(Var, Ref) \
ADD_ITEM (SEQID_ON_BIOSEQ, Var, Ref)

/// ERASE_SEQID_ON_BIOSEQ

#define ERASE_SEQID_ON_BIOSEQ(Itr, Var) \
LIST_ERASE_ITEM (SEQID_ON_BIOSEQ, Itr, Var)

/// SEQID_ON_BIOSEQ_IS_SORTED

#define SEQID_ON_BIOSEQ_IS_SORTED(Var, Func) \
IS_SORTED (SEQID_ON_BIOSEQ, Var, Func)

/// SORT_SEQID_ON_BIOSEQ

#define SORT_SEQID_ON_BIOSEQ(Var, Func) \
DO_LIST_SORT (SEQID_ON_BIOSEQ, Var, Func)

/// SEQID_ON_BIOSEQ_IS_UNIQUE

#define SEQID_ON_BIOSEQ_IS_UNIQUE(Var, Func) \
IS_UNIQUE (SEQID_ON_BIOSEQ, Var, Func)

/// UNIQUE_SEQID_ON_BIOSEQ

#define UNIQUE_SEQID_ON_BIOSEQ(Var, Func) \
DO_UNIQUE (SEQID_ON_BIOSEQ, Var, Func)


///
/// CSeq_annot macros

/// SEQANNOT_CHOICE macros

#define SEQANNOT_CHOICE_Test(Var) (Var).IsSetData()
#define SEQANNOT_CHOICE_Chs(Var)  (Var).GetData().Which()

/// SEQANNOT_CHOICE_IS

#define SEQANNOT_CHOICE_IS(Var, Chs) \
CHOICE_IS (SEQANNOT_CHOICE, Var, Chs)

/// SEQANNOT_IS_FTABLE

#define SEQANNOT_IS_FTABLE(Var) \
SEQANNOT_CHOICE_IS (Var, NCBI_SEQANNOT(Ftable))

/// SEQANNOT_IS_ALIGN

#define SEQANNOT_IS_ALIGN(Var) \
SEQANNOT_CHOICE_IS (Var, NCBI_SEQANNOT(Align))

/// SEQANNOT_IS_GRAPH

#define SEQANNOT_IS_GRAPH(Var) \
SEQANNOT_CHOICE_IS (Var, NCBI_SEQANNOT(Graph))

/// SEQANNOT_IS_IDS

#define SEQANNOT_IS_IDS(Var) \
SEQANNOT_CHOICE_IS (Var, NCBI_SEQANNOT(Ids))

/// SEQANNOT_IS_LOCS

#define SEQANNOT_IS_LOCS(Var) \
SEQANNOT_CHOICE_IS (Var, NCBI_SEQANNOT(Locs))

/// SEQANNOT_IS_SEQ_TABLE

#define SEQANNOT_IS_SEQ_TABLE(Var) \
SEQANNOT_CHOICE_IS (Var, NCBI_SEQANNOT(Seq_table))

/// SWITCH_ON_SEQANNOT_CHOICE

#define SWITCH_ON_SEQANNOT_CHOICE(Var) \
SWITCH_ON (SEQANNOT_CHOICE, Var)


/// SEQFEAT_ON_SEQANNOT macros

#define SEQFEAT_ON_SEQANNOT_Type      CSeq_annot::TData::TFtable
#define SEQFEAT_ON_SEQANNOT_Test(Var) (Var).IsFtable()
#define SEQFEAT_ON_SEQANNOT_Get(Var)  (Var).GetData().GetFtable()
#define SEQFEAT_ON_SEQANNOT_Set(Var)  (Var).SetData().SetFtable()

/// SEQANNOT_IS_SEQFEAT

#define SEQANNOT_IS_SEQFEAT(Var) \
ITEM_HAS (SEQFEAT_ON_SEQANNOT, Var)

/// FOR_EACH_SEQFEAT_ON_SEQANNOT
/// EDIT_EACH_SEQFEAT_ON_SEQANNOT
// CSeq_annot& as input, dereference with [const] CSeq_feat& feat = **itr;

#define FOR_EACH_SEQFEAT_ON_SEQANNOT(Itr, Var) \
FOR_EACH (SEQFEAT_ON_SEQANNOT, Itr, Var)

#define EDIT_EACH_SEQFEAT_ON_SEQANNOT(Itr, Var) \
EDIT_EACH (SEQFEAT_ON_SEQANNOT, Itr, Var)
 
/// ADD_SEQFEAT_TO_SEQANNOT

#define ADD_SEQFEAT_TO_SEQANNOT(Var, Ref) \
ADD_ITEM (SEQFEAT_ON_SEQANNOT, Var, Ref)

/// ERASE_SEQFEAT_ON_SEQANNOT
 
#define ERASE_SEQFEAT_ON_SEQANNOT(Itr, Var) \
LIST_ERASE_ITEM (SEQFEAT_ON_SEQANNOT, Itr, Var)

/// ANNOT_IS_FEATURE
/// FOR_EACH_FEATURE_ON_ANNOT
/// EDIT_EACH_FEATURE_ON_ANNOT
/// ADD_FEATURE_TO_ANNOT
/// ERASE_FEATURE_ON_ANNOT

#define ANNOT_IS_FEATURE SEQANNOT_IS_SEQFEAT
#define FOR_EACH_FEATURE_ON_ANNOT FOR_EACH_SEQFEAT_ON_SEQANNOT
#define EDIT_EACH_FEATURE_ON_ANNOT EDIT_EACH_SEQFEAT_ON_SEQANNOT
#define ADD_FEATURE_TO_ANNOT ADD_SEQFEAT_TO_SEQANNOT
#define ERASE_FEATURE_ON_ANNOT ERASE_SEQFEAT_ON_SEQANNOT


/// SEQALIGN_ON_SEQANNOT macros

#define SEQALIGN_ON_SEQANNOT_Type      CSeq_annot::TData::TAlign
#define SEQALIGN_ON_SEQANNOT_Test(Var) (Var).IsAlign()
#define SEQALIGN_ON_SEQANNOT_Get(Var)  (Var).GetData().GetAlign()
#define SEQALIGN_ON_SEQANNOT_Set(Var)  (Var).SetData().SetAlign()

/// SEQANNOT_IS_SEQALIGN

#define SEQANNOT_IS_SEQALIGN(Var) \
ITEM_HAS (SEQALIGN_ON_SEQANNOT, Var)

/// FOR_EACH_SEQALIGN_ON_SEQANNOT
/// EDIT_EACH_SEQALIGN_ON_SEQANNOT
// CSeq_annot& as input, dereference with [const] CSeq_align& align = **itr;

#define FOR_EACH_SEQALIGN_ON_SEQANNOT(Itr, Var) \
FOR_EACH (SEQALIGN_ON_SEQANNOT, Itr, Var)

#define EDIT_EACH_SEQALIGN_ON_SEQANNOT(Itr, Var) \
EDIT_EACH (SEQALIGN_ON_SEQANNOT, Itr, Var)

/// ADD_SEQALIGN_TO_SEQANNOT

#define ADD_SEQALIGN_TO_SEQANNOT(Var, Ref) \
ADD_ITEM (SEQALIGN_ON_SEQANNOT, Var, Ref)

/// ERASE_SEQALIGN_ON_SEQANNOT

#define ERASE_SEQALIGN_ON_SEQANNOT(Itr, Var) \
LIST_ERASE_ITEM (SEQALIGN_ON_SEQANNOT, Itr, Var)

/// ANNOT_IS_ALIGN
/// FOR_EACH_ALIGN_ON_ANNOT
/// EDIT_EACH_ALIGN_ON_ANNOT
/// ADD_ALIGN_TO_ANNOT
/// ERASE_ALIGN_ON_ANNOT

#define ANNOT_IS_ALIGN SEQANNOT_IS_SEQALIGN
#define FOR_EACH_ALIGN_ON_ANNOT FOR_EACH_SEQALIGN_ON_SEQANNOT
#define EDIT_EACH_ALIGN_ON_ANNOT EDIT_EACH_SEQALIGN_ON_SEQANNOT
#define ADD_ALIGN_TO_ANNOT ADD_SEQALIGN_TO_SEQANNOT
#define ERASE_ALIGN_ON_ANNOT ERASE_SEQALIGN_ON_SEQANNOT


/// SEQGRAPH_ON_SEQANNOT macros

#define SEQGRAPH_ON_SEQANNOT_Type      CSeq_annot::TData::TGraph
#define SEQGRAPH_ON_SEQANNOT_Test(Var) (Var).IsGraph()
#define SEQGRAPH_ON_SEQANNOT_Get(Var)  (Var).GetData().GetGraph()
#define SEQGRAPH_ON_SEQANNOT_Set(Var)  SetData().SetGraph()

/// SEQANNOT_IS_SEQGRAPH

#define SEQANNOT_IS_SEQGRAPH(Var) \
ITEM_HAS (SEQGRAPH_ON_SEQANNOT, Var)

/// FOR_EACH_SEQGRAPH_ON_SEQANNOT
/// EDIT_EACH_SEQGRAPH_ON_SEQANNOT
// CSeq_annot& as input, dereference with [const] CSeq_graph& graph = **itr;

#define FOR_EACH_SEQGRAPH_ON_SEQANNOT(Itr, Var) \
FOR_EACH (SEQGRAPH_ON_SEQANNOT, Itr, Var)

#define EDIT_EACH_SEQGRAPH_ON_SEQANNOT(Itr, Var) \
EDIT_EACH (SEQGRAPH_ON_SEQANNOT, Itr, Var)

/// ADD_SEQGRAPH_TO_SEQANNOT

#define ADD_SEQGRAPH_TO_SEQANNOT(Var, Ref) \
ADD_ITEM (SEQGRAPH_ON_SEQANNOT, Var, Ref)

/// ERASE_SEQGRAPH_ON_SEQANNOT

#define ERASE_SEQGRAPH_ON_SEQANNOT(Itr, Var) \
LIST_ERASE_ITEM (SEQGRAPH_ON_SEQANNOT, Itr, Var)

/// ANNOT_IS_GRAPH
/// FOR_EACH_GRAPH_ON_ANNOT
/// EDIT_EACH_GRAPH_ON_ANNOT
/// ADD_GRAPH_TO_ANNOT
/// ERASE_GRAPH_ON_ANNOT

#define ANNOT_IS_GRAPH SEQANNOT_IS_SEQGRAPH
#define FOR_EACH_GRAPH_ON_ANNOT FOR_EACH_SEQGRAPH_ON_SEQANNOT
#define EDIT_EACH_GRAPH_ON_ANNOT EDIT_EACH_SEQGRAPH_ON_SEQANNOT
#define ADD_GRAPH_TO_ANNOT ADD_SEQGRAPH_TO_SEQANNOT
#define ERASE_GRAPH_ON_ANNOT ERASE_SEQGRAPH_ON_SEQANNOT


/// SEQTABLE_ON_SEQANNOT macros

#define SEQTABLE_ON_SEQANNOT_Type      CSeq_annot::TData::TSeq_table
#define SEQTABLE_ON_SEQANNOT_Test(Var) (Var).IsSeq_table()
#define SEQTABLE_ON_SEQANNOT_Get(Var)  (Var).GetData().GetSeq_table()
#define SEQTABLE_ON_SEQANNOT_Set(Var)  SetData().SetSeq_table()

/// SEQANNOT_IS_SEQTABLE

#define SEQANNOT_IS_SEQTABLE(Var) \
ITEM_HAS (SEQTABLE_ON_SEQANNOT, Var)

/// ANNOT_IS_TABLE

#define ANNOT_IS_TABLE SEQANNOT_IS_SEQTABLE


/// ANNOTDESC_ON_SEQANNOT macros

#define ANNOTDESC_ON_SEQANNOT_Type      CSeq_annot::TDesc::Tdata
#define ANNOTDESC_ON_SEQANNOT_Test(Var) (Var).IsSetDesc() && (Var).GetDesc().IsSet()
#define ANNOTDESC_ON_SEQANNOT_Get(Var)  (Var).GetDesc().Get()
#define ANNOTDESC_ON_SEQANNOT_Set(Var)  (Var).SetDesc().Set()

/// SEQANNOT_HAS_ANNOTDESC

#define SEQANNOT_HAS_ANNOTDESC(Var) \
ITEM_HAS (ANNOTDESC_ON_SEQANNOT, Var)

/// FOR_EACH_ANNOTDESC_ON_SEQANNOT
/// EDIT_EACH_ANNOTDESC_ON_SEQANNOT
// CSeq_annot& as input, dereference with [const] CAnnotdesc& desc = **itr;

#define FOR_EACH_ANNOTDESC_ON_SEQANNOT(Itr, Var) \
FOR_EACH (ANNOTDESC_ON_SEQANNOT, Itr, Var)

#define EDIT_EACH_ANNOTDESC_ON_SEQANNOT(Itr, Var) \
EDIT_EACH (ANNOTDESC_ON_SEQANNOT, Itr, Var)

/// ADD_ANNOTDESC_TO_SEQANNOT

#define ADD_ANNOTDESC_TO_SEQANNOT(Var, Ref) \
ADD_ITEM (ANNOTDESC_ON_SEQANNOT, Var, Ref)

/// ERASE_ANNOTDESC_ON_SEQANNOT

#define ERASE_ANNOTDESC_ON_SEQANNOT(Itr, Var) \
LIST_ERASE_ITEM (ANNOTDESC_ON_SEQANNOT, Itr, Var)

/// ANNOT_HAS_ANNOTDESC
/// FOR_EACH_ANNOTDESC_ON_ANNOT
/// EDIT_EACH_ANNOTDESC_ON_ANNOT
/// ADD_ANNOTDESC_TO_ANNOT
/// ERASE_ANNOTDESC_ON_ANNOT

#define ANNOT_HAS_ANNOTDESC SEQANNOT_HAS_ANNOTDESC
#define FOR_EACH_ANNOTDESC_ON_ANNOT FOR_EACH_ANNOTDESC_ON_SEQANNOT
#define EDIT_EACH_ANNOTDESC_ON_ANNOT EDIT_EACH_ANNOTDESC_ON_SEQANNOT
#define ADD_ANNOTDESC_TO_ANNOT ADD_ANNOTDESC_TO_SEQANNOT
#define ERASE_ANNOTDESC_ON_ANNOT ERASE_ANNOTDESC_ON_SEQANNOT

///
/// SEQFEAT_ON_MAPEXT macros

#define SEQFEAT_ON_MAPEXT_Type      CMap_ext::Tdata
#define SEQFEAT_ON_MAPEXT_Test(Var) (Var).IsSet()
#define SEQFEAT_ON_MAPEXT_Get(Var)  (Var).Get()
#define SEQFEAT_ON_MAPEXT_Set(Var)  (Var).Set()

/// SEQANNOT_IS_SEQFEAT

/// FOR_EACH_SEQFEAT_ON_MAPEXT
/// EDIT_EACH_SEQFEAT_ON_MAPEXT

#define FOR_EACH_SEQFEAT_ON_MAPEXT(Itr, Var) \
FOR_EACH (SEQFEAT_ON_MAPEXT, Itr, Var)

#define EDIT_EACH_SEQFEAT_ON_MAPEXT(Itr, Var) \
EDIT_EACH (SEQFEAT_ON_MAPEXT, Itr, Var)
 
/// ADD_SEQFEAT_ON_MAPEXT

#define ADD_SEQFEAT_TO_MAPEXT(Var, Ref) \
ADD_ITEM (SEQFEAT_ON_MAPEXT, Var, Ref)

/// ERASE_SEQFEAT_ON_MAPEXT
 
#define ERASE_SEQFEAT_ON_MAPEXT(Itr, Var) \
LIST_ERASE_ITEM (SEQFEAT_ON_MAPEXT, Itr, Var)

///
/// CAnnotdesc macros

/// ANNOTDESC_CHOICE macros

#define ANNOTDESC_CHOICE_Test(Var) (Var).Which() != CAnnotdesc::e_not_set
#define ANNOTDESC_CHOICE_Chs(Var)  (Var).Which()

/// ANNOTDESC_CHOICE_IS

#define ANNOTDESC_CHOICE_IS(Var, Chs) \
CHOICE_IS (ANNOTDESC_CHOICE, Var, Chs)

/// SWITCH_ON_ANNOTDESC_CHOICE

#define SWITCH_ON_ANNOTDESC_CHOICE(Var) \
SWITCH_ON (ANNOTDESC_CHOICE, Var)


///
/// CSeq_descr macros

/// SEQDESC_ON_SEQDESCR macros

#define SEQDESC_ON_SEQDESCR_Type      CSeq_descr::Tdata
#define SEQDESC_ON_SEQDESCR_Test(Var) (Var).IsSet()
#define SEQDESC_ON_SEQDESCR_Get(Var)  (Var).Get()
#define SEQDESC_ON_SEQDESCR_Set(Var)  (Var).Set()

/// SEQDESCR_HAS_SEQDESC

#define SEQDESCR_HAS_SEQDESC(Var) \
ITEM_HAS (SEQDESC_ON_SEQDESCR, Var)

/// FOR_EACH_SEQDESC_ON_SEQDESCR
/// EDIT_EACH_SEQDESC_ON_SEQDESCR
// CSeq_descr& as input, dereference with [const] CSeqdesc& desc = **itr;

#define FOR_EACH_SEQDESC_ON_SEQDESCR(Itr, Var) \
FOR_EACH (SEQDESC_ON_SEQDESCR, Itr, Var)

#define EDIT_EACH_SEQDESC_ON_SEQDESCR(Itr, Var) \
EDIT_EACH (SEQDESC_ON_SEQDESCR, Itr, Var)

/// ADD_SEQDESC_TO_SEQDESCR

#define ADD_SEQDESC_TO_SEQDESCR(Var, Ref) \
ADD_ITEM (SEQDESC_ON_SEQDESCR, Var, Ref)

/// ERASE_SEQDESC_ON_SEQDESCR

#define ERASE_SEQDESC_ON_SEQDESCR(Itr, Var) \
LIST_ERASE_ITEM (SEQDESC_ON_SEQDESCR, Itr, Var)

/// DESCR_HAS_DESCRIPTOR
/// FOR_EACH_DESCRIPTOR_ON_DESCR
/// EDIT_EACH_DESCRIPTOR_ON_DESCR
/// ADD_DESCRIPTOR_TO_DESCR
/// ERASE_DESCRIPTOR_ON_DESCR

#define DESCR_HAS_DESCRIPTOR SEQDESCR_HAS_SEQDESC
#define FOR_EACH_DESCRIPTOR_ON_DESCR FOR_EACH_SEQDESC_ON_SEQDESCR
#define EDIT_EACH_DESCRIPTOR_ON_DESCR EDIT_EACH_SEQDESC_ON_SEQDESCR
#define ERASE_DESCRIPTOR_ON_DESCR ERASE_SEQDESC_ON_SEQDESCR
#define ADD_DESCRIPTOR_TO_DESCR ADD_SEQDESC_TO_SEQDESCR


///
/// CSeqdesc macros

/// SEQDESC_CHOICE macros

#define SEQDESC_CHOICE_Test(Var) (Var).Which() != CSeqdesc::e_not_set
#define SEQDESC_CHOICE_Chs(Var)  (Var).Which()

/// SEQDESC_CHOICE_IS

#define SEQDESC_CHOICE_IS(Var, Chs) \
CHOICE_IS (SEQDESC_CHOICE, Var, Chs)

/// SWITCH_ON_SEQDESC_CHOICE

#define SWITCH_ON_SEQDESC_CHOICE(Var) \
SWITCH_ON (SEQDESC_CHOICE, Var)

/// DESCRIPTOR_CHOICE_IS
/// SWITCH_ON_DESCRIPTOR_CHOICE

#define DESCRIPTOR_CHOICE_IS SEQDESC_CHOICE_IS
#define SWITCH_ON_DESCRIPTOR_CHOICE SWITCH_ON_SEQDESC_CHOICE


///
/// CMolInfo macros

/// MOLINFO_BIOMOL macros

#define MOLINFO_BIOMOL_Test(Var) (Var).IsSetBiomol()
#define MOLINFO_BIOMOL_Chs(Var)  (Var).GetBiomol()

/// MOLINFO_BIOMOL_IS

#define MOLINFO_BIOMOL_IS(Var, Chs) \
CHOICE_IS (MOLINFO_BIOMOL, Var, Chs)

/// SWITCH_ON_MOLINFO_BIOMOL

#define SWITCH_ON_MOLINFO_BIOMOL(Var) \
SWITCH_ON (MOLINFO_BIOMOL, Var)


/// MOLINFO_TECH macros

#define MOLINFO_TECH_Test(Var) (Var).IsSetTech()
#define MOLINFO_TECH_Chs(Var)  (Var).GetTech()

/// MOLINFO_TECH_IS

#define MOLINFO_TECH_IS(Var, Chs) \
CHOICE_IS (MOLINFO_TECH, Var, Chs)

/// SWITCH_ON_MOLINFO_TECH

#define SWITCH_ON_MOLINFO_TECH(Var) \
SWITCH_ON (MOLINFO_TECH, Var)


/// MOLINFO_COMPLETENESS macros

#define MOLINFO_COMPLETENESS_Test(Var) (Var).IsSetCompleteness()
#define MOLINFO_COMPLETENESS_Chs(Var)  (Var).GetCompleteness()

/// MOLINFO_COMPLETENESS_IS

#define MOLINFO_COMPLETENESS_IS(Var, Chs) \
CHOICE_IS (MOLINFO_COMPLETENESS, Var, Chs)

/// SWITCH_ON_MOLINFO_COMPLETENESS

#define SWITCH_ON_MOLINFO_COMPLETENESS(Var) \
SWITCH_ON (MOLINFO_COMPLETENESS, Var)


///
/// CDelta_ext macros

#define DELTASEQ_IN_DELTAEXT_Type       list< CRef< CDelta_seq > >
#define DELTASEQ_IN_DELTAEXT_Test(Var)  (  (Var).IsSet() && ! (Var).Get().empty() )
#define DELTASEQ_IN_DELTAEXT_Get(Var)   (Var).Get()
#define DELTASEQ_IN_DELTAEXT_Set(Var)   (Var).Set()
#define DELTASEQ_IN_DELTAEXT_Reset(Var) (Var).Reset()

#define FOR_EACH_DELTASEQ_IN_DELTAEXT(Itr, Var) \
    FOR_EACH( DELTASEQ_IN_DELTAEXT, Itr, Var )

#define EDIT_EACH_DELTASEQ_IN_DELTAEXT(Itr, Var) \
    EDIT_EACH( DELTASEQ_IN_DELTAEXT, Itr, Var )

#define ERASE_DELTASEQ_IN_DELTAEXT(Itr, Var) \
    LIST_ERASE_ITEM (DELTASEQ_IN_DELTAEXT, Itr, Var)


END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */

#endif  /* OBJECTS_SEQ___SEQ_MACROS__HPP */
