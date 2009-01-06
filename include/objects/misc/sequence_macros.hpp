#ifndef __SEQUENCE_MACROS__HPP__
#define __SEQUENCE_MACROS__HPP__

/*
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
* Author: Jonathan Kans
*
* File Description: Utility macros for exploring NCBI objects
*
* ===========================================================================
*/


#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/submit/submit__.hpp>
#include <objects/seqblock/seqblock__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/biblio/biblio__.hpp>
#include <objects/pub/pub__.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/// @NAME Convenience macros for NCBI object iterators
/// @{


/////////////////////////////////////////////////////////////////////////////
/// Macros and typedefs for object subtypes
/////////////////////////////////////////////////////////////////////////////


/// CSeq_entry definitions

#define NCBI_SEQENTRY(TYPE) CSeq_entry::e_##Type
typedef CSeq_entry::E_Choice TSEQENTRY_CHOICE;


/// CSeq_id definitions

#define NCBI_SEQID(Type) CSeq_id::e_##Type
typedef CSeq_id::E_Choice TSEQID_CHOICE;

#define NCBI_ACCN(Type) CSeq_id::eAcc_##Type
typedef CSeq_id::EAccessionInfo TACCN_CHOICE;


/// CSeq_inst definitions

#define NCBI_SEQREPR(Type) CSeq_inst::eRepr_##Type
typedef CSeq_inst::TRepr TSEQ_REPR;

#define NCBI_SEQMOL(Type) CSeq_inst::eMol_##Type
typedef CSeq_inst::TMol TSEQ_MOL;

#define NCBI_SEQTOPOLOGY(Type) CSeq_inst::eTopology_##Type
typedef CSeq_inst::TTopology TSEQ_TOPOLOGY;


/// CAnnotdesc definitions

#define NCBI_ANNOTDESC(Type) CAnnotdesc::e_##Type
typedef CAnnotdesc::E_Choice TANNOTDESC_CHOICE;


/// CSeqdesc definitions

#define NCBI_SEQDESC(Type) CSeqdesc::e_##Type
typedef CSeqdesc::E_Choice TSEQDESC_CHOICE;


/// CMolInfo definitions

#define NCBI_BIOMOL(Type) CMolInfo::eBiomol_##Type
typedef CMolInfo::TBiomol TMOLINFO_BIOMOL;

#define NCBI_TECH(Type) CMolInfo::eTech_##Type
typedef CMolInfo::TTech TMOLINFO_TECH;

#define NCBI_COMPLETENESS(Type) CMolInfo::eCompleteness_##Type
typedef CMolInfo::TCompleteness TMOLINFO_COMPLETENESS;


/// CBioSource definitions

#define NCBI_GENOME(Type) CBioSource::eGenome_##Type
typedef CBioSource::TGenome TBIOSOURCE_GENOME;

#define NCBI_ORIGIN(Type) CBioSource::eOrigin_##Type
typedef CBioSource::TOrigin TBIOSOURCE_ORIGIN;


/// CSubSource definitions

#define NCBI_SUBSRC(Type) CSubSource::eSubtype_##Type
typedef CSubSource::TSubtype TSUBSRC_SUBTYPE;


/// COrgMod definitions

#define NCBI_ORGMOD(Type) COrgMod::eSubtype_##Type
typedef COrgMod::TSubtype TORGMOD_SUBTYPE;


/// CPub definitions

#define NCBI_PUB(Type) CPub::e_##Type
typedef CPub::E_Choice TPUB_CHOICE;


/// CSeq_feat definitions

#define NCBI_SEQFEAT(Type) CSeqFeatData::e_##Type
typedef CSeqFeatData::E_Choice TSEQFEAT_CHOICE;


/////////////////////////////////////////////////////////////////////////////
/// Macros for obtaining closest specific CSeqdesc applying to a CBioseq
/////////////////////////////////////////////////////////////////////////////


/// IF_EXISTS_CLOSEST_MOLINFO
// Takes const CBioseq& as input and makes reference to CConstRef<CSeqdesc>
// Dereference with const CMolInfo& molinf = (*cref).GetMolinfo();
// If Lvl is not NULL, it must be a pointer to an int

#define IF_EXISTS_CLOSEST_MOLINFO(Cref, Bsq, Lvl) \
if (CConstRef<CSeqdesc> Cref = (Bsq).GetClosestDescriptor (CSeqdesc::e_Molinfo, Lvl))

/// IF_EXISTS_CLOSEST_BIOSOURCE
// Takes const CBioseq& as input and makes reference to CConstRef<CSeqdesc>
// Dereference with const CBioSource& source = (*cref).GetSource();
// If Lvl is not NULL, it must be a pointer to an int

#define IF_EXISTS_CLOSEST_BIOSOURCE(Cref, Bsq, Lvl) \
if (CConstRef<CSeqdesc> Cref = (Bsq).GetClosestDescriptor (CSeqdesc::e_Source, Lvl))

/// IF_EXISTS_CLOSEST_TITLE
// Takes const CBioseq& as input and makes reference to CConstRef<CSeqdesc>
// Dereference with const string& title = (*cref).GetTitle();
// If Lvl is not NULL, it must be a pointer to an int

#define IF_EXISTS_CLOSEST_TITLE(Cref, Bsq, Lvl) \
if (CConstRef<CSeqdesc> Cref = (Bsq).GetClosestDescriptor (CSeqdesc::e_Title, Lvl))

/// IF_EXISTS_CLOSEST_GENBANKBLOCK
// Takes const CBioseq& as input and makes reference to CConstRef<CSeqdesc>
// Dereference with const CGB_block& gbk = (*cref).GetGenbank();
// If Lvl is not NULL, it must be a pointer to an int

#define IF_EXISTS_CLOSEST_GENBANKBLOCK(Cref, Bsq, Lvl) \
if (CConstRef<CSeqdesc> Cref = (Bsq).GetClosestDescriptor (CSeqdesc::e_Genbank, Lvl))

/// IF_EXISTS_CLOSEST_EMBLBLOCK
// Takes const CBioseq& as input and makes reference to CConstRef<CSeqdesc>
// Dereference with const CEMBL_block& ebk = (*cref).GetEmbl();
// If Lvl is not NULL, it must be a pointer to an int

#define IF_EXISTS_CLOSEST_EMBLBLOCK(Cref, Bsq, Lvl) \
if (CConstRef<CSeqdesc> Cref = (Bsq).GetClosestDescriptor (CSeqdesc::e_Embl, Lvl))

/// IF_EXISTS_CLOSEST_PDBBLOCK
// Takes const CBioseq& as input and makes reference to CConstRef<CSeqdesc>
// Dereference with const CPDB_block& pbk = (*cref).GetPdb();
// If Lvl is not NULL, it must be a pointer to an int

#define IF_EXISTS_CLOSEST_PDBBLOCK(Cref, Bsq, Lvl) \
if (CConstRef<CSeqdesc> Cref = (Bsq).GetClosestDescriptor (CSeqdesc::e_Pdb, Lvl))


/////////////////////////////////////////////////////////////////////////////
/// Macros to recursively explore within NCBI data model objects
/////////////////////////////////////////////////////////////////////////////


/// NCBI_SERIAL_TEST_EXPLORE base macro tests to see if loop should be entered
// If okay, calls CTypeConstIterator for recursive exploration

#define NCBI_SERIAL_TEST_EXPLORE(Test, Type, Var, Cont) \
if (! (Test)) {} else for (CTypeConstIterator<Type> Var (Cont); Var; ++Var)


// "VISIT_ALL_XXX_WITHIN_YYY" does a recursive exploration of NCBI objects


/// CSeq_entry explorers

/// VISIT_ALL_SEQENTRYS_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeq_entry&
// Dereference with const CSeq_entry& seqentry = *iter;

#define VISIT_ALL_SEQENTRYS_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeq_entry, \
    Iter, \
    (Seq))

/// VISIT_ALL_BIOSEQS_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CBioseq&
// Dereference with const CBioseq& bioseq = *iter;

#define VISIT_ALL_BIOSEQS_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CBioseq, \
    Iter, \
    (Seq))

/// VISIT_ALL_SEQSETS_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CBioseq_set&
// Dereference with const CBioseq_set& bss = *iter;

#define VISIT_ALL_SEQSETS_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CBioseq_set, \
    Iter, \
    (Seq))

/// VISIT_ALL_DESCRIPTORS_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeqdesc&
// Dereference with const CSeqdesc& desc = *iter;

#define VISIT_ALL_DESCRIPTORS_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeqdesc, \
    Iter, \
    (Seq))

/// VISIT_ALL_ANNOTS_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeq_annot&
// Dereference with const CSeq_annot& annot = *iter;

#define VISIT_ALL_ANNOTS_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeq_annot, \
    Iter, \
    (Seq))

/// VISIT_ALL_FEATURES_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeq_feat&
// Dereference with const CSeq_feat& feat = *iter;

#define VISIT_ALL_FEATURES_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeq_feat, \
    Iter, \
    (Seq))

/// VISIT_ALL_ALIGNS_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeq_feat&
// Dereference with const CSeq_align& align = *iter;

#define VISIT_ALL_ALIGNS_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeq_align, \
    Iter, \
    (Seq))

/// VISIT_ALL_GRAPHS_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeq_feat&
// Dereference with const CSeq_graph& graph = *iter;

#define VISIT_ALL_GRAPHS_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeq_graph, \
    Iter, \
    (Seq))


/// CBioseq_set explorers

/// VISIT_ALL_SEQENTRYS_WITHIN_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_entry&
// Dereference with const CSeq_entry& seqentry = *iter;

#define VISIT_ALL_SEQENTRYS_WITHIN_SEQSET(Iter, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CSeq_entry, \
    Iter, \
    (Bss))

/// VISIT_ALL_BIOSEQS_WITHIN_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CBioseq&
// Dereference with const CBioseq& bioseq = *iter;

#define VISIT_ALL_BIOSEQS_WITHIN_SEQSET(Iter, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CBioseq, \
    Iter, \
    (Bss))

/// VISIT_ALL_SEQSETS_WITHIN_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CBioseq_set&
// Dereference with const CBioseq_set& bss = *iter;

#define VISIT_ALL_SEQSETS_WITHIN_SEQSET(Iter, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CBioseq_set, \
    Iter, \
    (Bss))

/// VISIT_ALL_DESCRIPTORS_WITHIN_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeqdesc&
// Dereference with const CSeqdesc& desc = *iter;

#define VISIT_ALL_DESCRIPTORS_WITHIN_SEQSET(Iter, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CSeqdesc, \
    Iter, \
    (Bss))

/// VISIT_ALL_ANNOTS_WITHIN_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_annot&
// Dereference with const CSeq_annot& annot = *iter;

#define VISIT_ALL_ANNOTS_WITHIN_SEQSET(Iter, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CSeq_annot, \
    Iter, \
    (Bss))

/// VISIT_ALL_FEATURES_WITHIN_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_feat&
// Dereference with const CSeq_feat& feat = *iter;

#define VISIT_ALL_FEATURES_WITHIN_SEQSET(Iter, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CSeq_feat, \
    Iter, \
    (Bss))

/// VISIT_ALL_ALIGNS_WITHIN_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_feat&
// Dereference with const CSeq_align& align = *iter;

#define VISIT_ALL_ALIGNS_WITHIN_SEQSET(Iter, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CSeq_align, \
    Iter, \
    (Bss))

/// VISIT_ALL_GRAPHS_WITHIN_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_feat&
// Dereference with const CSeq_graph& graph = *iter;

#define VISIT_ALL_GRAPHS_WITHIN_SEQSET(Iter, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CSeq_graph, \
    Iter, \
    (Bss))



/////////////////////////////////////////////////////////////////////////////
/// Macros to iterate over standard template containers (non-recursive)
/////////////////////////////////////////////////////////////////////////////


/// NCBI_CS_ITERATE base macro tests to see if loop should be entered
// If okay, calls ITERATE for linear STL iteration

#define NCBI_CS_ITERATE(Test, Type, Var, Cont) \
if (! (Test)) {} else ITERATE(Type, Var, Cont)

/// NCBI_NC_ITERATE base macro tests to see if loop should be entered
// If okay, calls NON_CONST_ITERATE for linear STL iteration

#define NCBI_NC_ITERATE(Test, Type, Var, Cont) \
if (! (Test)) {} else NON_CONST_ITERATE(Type, Var, Cont)

/// NCBI_SWITCH base macro tests to see if switch should be performed
// If okay, calls switch statement

#define NCBI_SWITCH(Test, Chs) \
if (! (Test)) {} else switch(Chs)


// "FOR_EACH_XXX_ON_YYY" does a linear const traversal of STL containers
// "EDIT_EACH_XXX_ON_YYY" does a linear non-const traversal of STL containers


/// CSeq_submit iterators

/// FOR_EACH_SEQENTRY_ON_SEQSUBMIT
// Takes const CSeq_submit& as input and makes iterator to const CSeq_entry&
// Dereference with const CSeq_entry& se = **iter;

#define FOR_EACH_SEQENTRY_ON_SEQSUBMIT(Iter, Ss) \
NCBI_CS_ITERATE( \
    (Ss).IsEntrys(), \
    CSeq_submit::TData::TEntrys, \
    Iter, \
    (Ss).GetData().GetEntrys())

/// EDIT_EACH_SEQENTRY_ON_SEQSUBMIT
// Takes const CSeq_submit& as input and makes iterator to const CSeq_entry&
// Dereference with CSeq_entry& se = **iter;

#define EDIT_EACH_SEQENTRY_ON_SEQSUBMIT(Iter, Ss) \
NCBI_NC_ITERATE( \
    (Ss).IsEntrys(), \
    CSeq_submit::TData::TEntrys, \
    Iter, \
    (Ss).SetData().SetEntrys())


/// CSeq_entry iterators

/// IF_SEQENTRY_IS_SEQ

#define IF_SEQENTRY_IS_SEQ(Seq) \
if ((Seq).IsSeq())

/// SEQENTRY_IS_SEQ

#define SEQENTRY_IS_SEQ(Seq) \
((Seq).IsSeq())

/// IF_SEQENTRY_IS_SET

#define IF_SEQENTRY_IS_SET(Seq) \
if ((Seq).IsSet())

/// SEQENTRY_IS_SET

#define SEQENTRY_IS_SET(Seq) \
((Seq).IsSet())

/// IF_SEQENTRY_CHOICE_IS

#define IF_SEQENTRY_CHOICE_IS(Seq, Chs) \
if ((Seq).Which() == Chs)

/// SEQENTRY_CHOICE_IS

#define SEQENTRY_CHOICE_IS(Seq, Chs) \
((Seq).Which() == Chs)

/// SWITCH_ON_SEQENTRY_CHOICE

#define SWITCH_ON_SEQENTRY_CHOICE(Seq) \
NCBI_SWITCH( \
    (Seq).Which() != CSeq_entry::e_not_set, \
    (Seq).Which())


/// IF_SEQENTRY_HAS_DESCRIPTOR

#define IF_SEQENTRY_HAS_DESCRIPTOR(Seq) \
if ((Seq).IsSetDescr())

/// SEQENTRY_HAS_DESCRIPTOR

#define SEQENTRY_HAS_DESCRIPTOR(Seq) \
((Seq).IsSetDescr())

/// FOR_EACH_DESCRIPTOR_ON_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeqdesc&
// Dereference with const CSeqdesc& desc = **iter

#define FOR_EACH_DESCRIPTOR_ON_SEQENTRY(Iter, Seq) \
NCBI_CS_ITERATE( \
    (Seq).IsSetDescr(), \
    CSeq_descr::Tdata, \
    Iter, \
    (Seq).GetDescr().Get())

/// EDIT_EACH_DESCRIPTOR_ON_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeqdesc&
// Dereference with CSeqdesc& desc = **iter

#define EDIT_EACH_DESCRIPTOR_ON_SEQENTRY(Iter, Seq) \
NCBI_NC_ITERATE( \
    (Seq).IsSetDescr(), \
    CSeq_descr::Tdata, \
    Iter, \
    (Seq).SetDescr().Set())


/// IF_SEQENTRY_HAS_ANNOT

#define IF_SEQENTRY_HAS_ANNOT(Seq) \
if ((Seq).IsSetAnnot())

/// SEQENTRY_HAS_ANNOT

#define SEQENTRY_HAS_ANNOT(Seq) \
((Seq).IsSetAnnot())

/// FOR_EACH_ANNOT_ON_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeq_annot&
// Dereference with const CSeq_annot& annot = **iter;

#define FOR_EACH_ANNOT_ON_SEQENTRY(Iter, Seq) \
NCBI_CS_ITERATE( \
    (Seq).IsSetAnnot(), \
    CSeq_entry::TAnnot, \
    Iter, \
    (Seq).GetAnnot())

/// EDIT_EACH_ANNOT_ON_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeq_annot&
// Dereference with CSeq_annot& annot = **iter;

#define EDIT_EACH_ANNOT_ON_SEQENTRY(Iter, Seq) \
NCBI_NC_ITERATE( \
    (Seq).IsSetAnnot(), \
    CSeq_entry::TAnnot, \
    Iter, \
    (Seq).SetAnnot())


/// CBioseq iterators

/// IF_BIOSEQ_HAS_DESCRIPTOR

#define IF_BIOSEQ_HAS_DESCRIPTOR(Bsq) \
if ((Bsq).IsSetDescr())

/// BIOSEQ_HAS_DESCRIPTOR

#define BIOSEQ_HAS_DESCRIPTOR(Bsq) \
((Bsq).IsSetDescr())

/// FOR_EACH_DESCRIPTOR_ON_BIOSEQ
// Takes const CBioseq& as input and makes iterator to const CSeqdesc&
// Dereference with const CSeqdesc& desc = **iter

#define FOR_EACH_DESCRIPTOR_ON_BIOSEQ(Iter, Bsq) \
NCBI_CS_ITERATE( \
    (Bsq).IsSetDescr(), \
    CBioseq::TDescr::Tdata, \
    Iter, \
    (Bsq).GetDescr().Get())

/// EDIT_EACH_DESCRIPTOR_ON_BIOSEQ
// Takes const CBioseq& as input and makes iterator to const CSeqdesc&
// Dereference with CSeqdesc& desc = **iter

#define EDIT_EACH_DESCRIPTOR_ON_BIOSEQ(Iter, Bsq) \
NCBI_NC_ITERATE( \
    (Bsq).IsSetDescr(), \
    CBioseq::TDescr::Tdata, \
    Iter, \
    (Bsq).SetDescr().Set())


/// IF_BIOSEQ_HAS_ANNOT

#define IF_BIOSEQ_HAS_ANNOT(Bsq) \
if ((Bsq).IsSetAnnot())

/// BIOSEQ_HAS_ANNOT

#define BIOSEQ_HAS_ANNOT(Bsq) \
((Bsq).IsSetAnnot())

/// FOR_EACH_ANNOT_ON_BIOSEQ
// Takes const CBioseq& as input and makes iterator to const CSeq_annot&
// Dereference with const CSeq_annot& annot = **iter;

#define FOR_EACH_ANNOT_ON_BIOSEQ(Iter, Bsq) \
NCBI_CS_ITERATE( \
    (Bsq).IsSetAnnot(), \
    CBioseq::TAnnot, \
    Iter, \
    (Bsq).GetAnnot())

/// EDIT_EACH_ANNOT_ON_BIOSEQ
// Takes const CBioseq& as input and makes iterator to const CSeq_annot&
// Dereference with CSeq_annot& annot = **iter;

#define EDIT_EACH_ANNOT_ON_BIOSEQ(Iter, Bsq) \
NCBI_NC_ITERATE( \
    (Bsq).IsSetAnnot(), \
    CBioseq::TAnnot, \
    Iter, \
    (Bsq).SetAnnot())


/// IF_BIOSEQ_HAS_SEQID

#define IF_BIOSEQ_HAS_SEQID(Bsq) \
if ((Bsq).IsSetId())

/// BIOSEQ_HAS_SEQID

#define BIOSEQ_HAS_SEQID(Bsq) \
((Bsq).IsSetId())

/// FOR_EACH_SEQID_ON_BIOSEQ
// Takes const CBioseq& as input and makes iterator to const CSeq_id&
// Dereference with const CSeq_id& sid = **iter;

#define FOR_EACH_SEQID_ON_BIOSEQ(Iter, Bsq) \
NCBI_CS_ITERATE( \
    (Bsq).IsSetId(), \
    CBioseq::TId, \
    Iter, \
    (Bsq).GetId())

/// EDIT_EACH_SEQID_ON_BIOSEQ
// Takes const CBioseq& as input and makes iterator to const CSeq_id&
// Dereference with CSeq_id& sid = **iter;

#define EDIT_EACH_SEQID_ON_BIOSEQ(Iter, Bsq) \
NCBI_NC_ITERATE( \
    (Bsq).IsSetId(), \
    CBioseq::TId, \
    Iter, \
    (Bsq).SetId())


/// CSeq_id iterators

/// IF_SEQID_CHOICE_IS

#define IF_SEQID_CHOICE_IS(Sid, Chs) \
if ((Sid).Which() == Chs)

/// SEQID_CHOICE_IS

#define SEQID_CHOICE_IS(Sid, Chs) \
((Sid).Which() == Chs)

/// SWITCH_ON_SEQID_CHOICE

#define SWITCH_ON_SEQID_CHOICE(Sid) \
NCBI_SWITCH( \
    (Sid).Which() != CSeq_id::e_not_set, \
    (Sid).Which())


/// CBioseq_set iterators

/// IF_SEQSET_HAS_DESCRIPTOR

#define IF_SEQSET_HAS_DESCRIPTOR(Bss) \
if ((Bss).IsSetDescr())

/// SEQSET_HAS_DESCRIPTOR

#define SEQSET_HAS_DESCRIPTOR(Bss) \
((Bss).IsSetDescr())

/// FOR_EACH_DESCRIPTOR_ON_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeqdesc&
// Dereference with const CSeqdesc& desc = **iter;

#define FOR_EACH_DESCRIPTOR_ON_SEQSET(Iter, Bss) \
NCBI_CS_ITERATE( \
    (Bss).IsSetDescr(), \
    CBioseq_set::TDescr::Tdata, \
    Iter, \
    (Bss).GetDescr().Get())

/// EDIT_EACH_DESCRIPTOR_ON_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeqdesc&
// Dereference with CSeqdesc& desc = **iter;

#define EDIT_EACH_DESCRIPTOR_ON_SEQSET(Iter, Bss) \
NCBI_NC_ITERATE( \
    (Bss).IsSetDescr(), \
    CBioseq_set::TDescr::Tdata, \
    Iter, \
    (Bss).SetDescr().Set())


/// IF_SEQSET_HAS_ANNOT

#define IF_SEQSET_HAS_ANNOT(Bss) \
if ((Bss).IsSetAnnot())

/// SEQSET_HAS_ANNOT

#define SEQSET_HAS_ANNOT(Bss) \
((Bss).IsSetAnnot())

/// FOR_EACH_ANNOT_ON_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_annot&
// Dereference with const CSeq_annot& annot = **iter;

#define FOR_EACH_ANNOT_ON_SEQSET(Iter, Bss) \
NCBI_CS_ITERATE( \
    (Bss).IsSetAnnot(), \
    CBioseq_set::TAnnot, \
    Iter, \
    (Bss).GetAnnot())

/// EDIT_EACH_ANNOT_ON_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_annot&
// Dereference with CSeq_annot& annot = **iter;

#define EDIT_EACH_ANNOT_ON_SEQSET(Iter, Bss) \
NCBI_NC_ITERATE( \
    (Bss).IsSetAnnot(), \
    CBioseq_set::TAnnot, \
    Iter, \
    (Bss).SetAnnot())


/// IF_SEQSET_HAS_SEQENTRY

#define IF_SEQSET_HAS_SEQENTRY(Bss) \
if ((Bss).IsSetSeq_set())

///SEQSET_HAS_SEQENTRY

#define SEQSET_HAS_SEQENTRY(Bss) \
((Bss).IsSetSeq_set())

/// FOR_EACH_SEQENTRY_ON_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_entry&
// Dereference with const CSeq_entry& se = **iter;

#define FOR_EACH_SEQENTRY_ON_SEQSET(Iter, Bss) \
NCBI_CS_ITERATE( \
    (Bss).IsSetSeq_set(), \
    CBioseq_set::TSeq_set, \
    Iter, \
    (Bss).GetSeq_set())

/// EDIT_EACH_SEQENTRY_ON_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_entry&
// Dereference with CSeq_entry& se = **iter;

#define EDIT_EACH_SEQENTRY_ON_SEQSET(Iter, Bss) \
NCBI_NC_ITERATE( \
    (Bss).IsSetSeq_set(), \
    CBioseq_set::TSeq_set, \
    Iter, \
    (Bss).SetSeq_set())


/// CSeq_annot iterators

/// IF_ANNOT_HAS_FEATURE

#define IF_ANNOT_HAS_FEATURE(San) \
if ((San).IsFtable())

/// ANNOT_IS_FEATURE

#define ANNOT_IS_FEATURE(San) \
if ((San).IsFtable())

/// FOR_EACH_FEATURE_ON_ANNOT
// Takes const CSeq_annot& as input and makes iterator to const CSeq_feat&
// Dereference with const CSeq_feat& feat = **iter;

#define FOR_EACH_FEATURE_ON_ANNOT(Iter, San) \
NCBI_CS_ITERATE( \
    (San).IsFtable(), \
    CSeq_annot::TData::TFtable, \
    Iter, \
    (San).GetData().GetFtable())

/// EDIT_EACH_FEATURE_ON_ANNOT
// Takes const CSeq_annot& as input and makes iterator to const CSeq_feat&
// Dereference with CSeq_feat& feat = **iter;

#define EDIT_EACH_FEATURE_ON_ANNOT(Iter, San) \
NCBI_NC_ITERATE( \
    (San).IsFtable(), \
    CSeq_annot::TData::TFtable, \
    Iter, \
    (San).SetData().SetFtable())


/// IF_ANNOT_HAS_ALIGN

#define IF_ANNOT_HAS_ALIGN(San) \
if ((San).IsAlign())

/// ANNOT_IS_ALIGN

#define ANNOT_IS_ALIGN(San) \
if ((San).IsAlign())

/// FOR_EACH_ALIGN_ON_ANNOT
// Takes const CSeq_annot& as input and makes iterator to const CSeq_align&
// Dereference with const CSeq_align& align = **iter;

#define FOR_EACH_ALIGN_ON_ANNOT(Iter, San) \
NCBI_CS_ITERATE( \
    (San).IsAlign(), \
    CSeq_annot::TData::TAlign, \
    Iter, \
    (San).GetData().GetAlign())

/// EDIT_EACH_ALIGN_ON_ANNOT
// Takes const CSeq_annot& as input and makes iterator to const CSeq_align&
// Dereference with CSeq_align& align = **iter;

#define EDIT_EACH_ALIGN_ON_ANNOT(Iter, San) \
NCBI_NC_ITERATE( \
    (San).IsAlign(), \
    CSeq_annot::TData::TAlign, \
    Iter, \
    (San).SetData().SetAlign())


/// IF_ANNOT_HAS_GRAPH

#define IF_ANNOT_HAS_GRAPH(San) \
if ((San).IsGraph())

/// ANNOT_IS_GRAPHS

#define ANNOT_IS_GRAPH(San) \
if ((San).IsGraph())

/// FOR_EACH_GRAPH_ON_ANNOT
// Takes const CSeq_annot& as input and makes iterator to const CSeq_graph&
// Dereference with const CSeq_graph& graph = **iter;

#define FOR_EACH_GRAPH_ON_ANNOT(Iter, San) \
NCBI_CS_ITERATE( \
    (San).IsGraph(), \
    CSeq_annot::TData::TGraph, \
    Iter, \
    (San).GetData().GetGraph())

/// EDIT_EACH_GRAPH_ON_ANNOT
// Takes const CSeq_annot& as input and makes iterator to const CSeq_graph&
// Dereference with CSeq_graph& graph = **iter;

#define EDIT_EACH_GRAPH_ON_ANNOT(Iter, San) \
NCBI_NC_ITERATE( \
    (San).IsGraph(), \
    CSeq_annot::TData::TGraph, \
    Iter, \
    (San).SetData().SetGraph())


/// IF_ANNOT_HAS_ANNOTDESC

#define IF_ANNOT_HAS_ANNOTDESC(San) \
if ((San).IsSetDesc() && (San).GetDesc().IsSet())

/// ANNOT_HAS_ANNOTDESC

#define ANNOT_HAS_ANNOTDESC(San) \
((San).IsSetDesc() && (San).GetDesc().IsSet())

/// FOR_EACH_ANNOTDESC_ON_ANNOT
// Takes const CSeq_annot& as input and makes iterator to const CAnnotdesc&
// Dereference with const CAnnotdesc& desc = **iter;

#define FOR_EACH_ANNOTDESC_ON_ANNOT(Iter, San) \
NCBI_CS_ITERATE( \
    (San).IsSetDesc() && (San).GetDesc().IsSet(), \
    CSeq_annot::TDesc::Tdata, \
    Iter, \
    (San).GetDesc().Get())

/// EDIT_EACH_ANNOTDESC_ON_ANNOT
// Takes const CSeq_annot& as input and makes iterator to const CAnnotdesc&
// Dereference with CAnnotdesc& desc = **iter;

#define EDIT_EACH_ANNOTDESC_ON_ANNOT(Iter, San) \
NCBI_NC_ITERATE( \
    (San).IsSetDesc(), \
    CSeq_annot::TDesc, \
    Iter, \
    (San).SetDesc())


/// CAnnotdesc iterators

/// IF_ANNOTDESC_CHOICE_IS

#define IF_ANNOTDESC_CHOICE_IS(Ads, Chs) \
if ((Ads).Which() == Chs)

/// ANNOTDESC_CHOICE_IS

#define ANNOTDESC_CHOICE_IS(Ads, Chs) \
((Ads).Which() == Chs)

/// SWITCH_ON_ANNOTDESC_CHOICE

#define SWITCH_ON_ANNOTDESC_CHOICE(Ads) \
NCBI_SWITCH( \
    (Ads).Which() != CAnnotdesc::e_not_set, \
    (Ads).Which())


/// CSeq_descr iterators

/// IF_DESCR_HAS_DESCRIPTOR

#define IF_DESCR_HAS_DESCRIPTOR(Descr) \
if ((Descr).IsSet())

/// DESCR_HAS_DESCRIPTOR

#define DESCR_HAS_DESCRIPTOR(Descr) \
((Descr).IsSet())

/// FOR_EACH_DESCRIPTOR_ON_DESCR
// Takes const CSeq_descr& as input and makes iterator to const CSeqdesc&
// Dereference with const CSeqdesc& desc = **iter;

#define FOR_EACH_DESCRIPTOR_ON_DESCR(Iter, Descr) \
NCBI_CS_ITERATE( \
    (Descr).IsSet(), \
    CSeq_descr::Tdata, \
    Iter, \
    (Descr).Get())

/// EDIT_EACH_DESCRIPTOR_ON_DESCR
// Takes const CSeq_descr& as input and makes iterator to const CSeqdesc&
// Dereference with CSeqdesc& desc = **iter;

#define EDIT_EACH_DESCRIPTOR_ON_DESCR(Iter, Descr) \
NCBI_NC_ITERATE( \
    (Descr).IsSet(), \
    CSeq_descr::Tdata, \
    Iter, \
    (Descr).Set())


/// CSeqdesc iterators

/// IF_DESCRIPTOR_CHOICE_IS

#define IF_DESCRIPTOR_CHOICE_IS(Dsc, Chs) \
if ((Dsc).Which() == Chs)

/// DESCRIPTOR_CHOICE_IS

#define DESCRIPTOR_CHOICE_IS(Dsc, Chs) \
((Dsc).Which() == Chs)

/// SWITCH_ON_DESCRIPTOR_CHOICE

#define SWITCH_ON_DESCRIPTOR_CHOICE(Dsc) \
NCBI_SWITCH( \
    (Dsc).Which() != CSeqdesc::e_not_set, \
    (Dsc).Which())


/// CMolInfo iterators

/// IF_MOLINFO_BIOMOL_IS

#define IF_MOLINFO_BIOMOL_IS(Mif, Chs) \
if ((Mif).IsSetBiomol() && (Mif).GetBiomol() == Chs)

/// MOLINFO_BIOMOL_IS

#define MOLINFO_BIOMOL_IS(Mif, Chs) \
((Mif).IsSetBiomol() && (Mif).GetBiomol() == Chs)

/// SWITCH_ON_MOLINFO_BIOMOL

#define SWITCH_ON_MOLINFO_BIOMOL(Mif) \
NCBI_SWITCH( \
    (Mif).IsSetBiomol(), \
    (Mif).GetBiomol())

/// IF_MOLINFO_TECH_IS

#define IF_MOLINFO_TECH_IS(Mif, Chs) \
if ((Mif).IsSetTech() && (Mif).GetTech() == Chs)

/// MOLINFO_TECH_IS

#define MOLINFO_TECH_IS(Mif, Chs) \
((Mif).IsSetTech() && (Mif).GetTech() == Chs)

/// SWITCH_ON_MOLINFO_TECH

#define SWITCH_ON_MOLINFO_TECH(Mif) \
NCBI_SWITCH( \
    (Mif).IsSetTech(), \
    (Mif).GetTech())

/// IF_MOLINFO_COMPLETENESS_IS

#define IF_MOLINFO_COMPLETENESS_IS(Mif, Chs) \
if ((Mif).IsSetCompleteness() && (Mif).GetCompleteness() == Chs)

/// MOLINFO_COMPLETENESS_IS

#define MOLINFO_COMPLETENESS_IS(Mif, Chs) \
((Mif).IsSetCompleteness() && (Mif).GetCompleteness() == Chs)

/// SWITCH_ON_MOLINFO_COMPLETENESS

#define SWITCH_ON_MOLINFO_COMPLETENESS(Mif) \
NCBI_SWITCH( \
    (Mif).IsSetCompleteness(), \
    (Mif).GetCompleteness())


/// CBioSource iterators

/// IF_BIOSOURCE_HAS_SUBSOURCE

#define IF_BIOSOURCE_HAS_SUBSOURCE(Bsrc) \
if ((Bsrc).IsSetSubtype())

/// BIOSOURCE_HAS_SUBSOURCE

#define BIOSOURCE_HAS_SUBSOURCE(Bsrc) \
((Bsrc).IsSetSubtype())

/// FOR_EACH_SUBSOURCE_ON_BIOSOURCE
// Takes const CBioSource& as input and makes iterator to const CSubSource&
// Dereference with const CSubSource& sbs = **iter

#define FOR_EACH_SUBSOURCE_ON_BIOSOURCE(Iter, Bsrc) \
NCBI_CS_ITERATE( \
    (Bsrc).IsSetSubtype(), \
    CBioSource::TSubtype, \
    Iter, \
    (Bsrc).GetSubtype())

/// EDIT_EACH_SUBSOURCE_ON_BIOSOURCE
// Takes const CBioSource& as input and makes iterator to const CSubSource&
// Dereference with CSubSource& sbs = **iter

#define EDIT_EACH_SUBSOURCE_ON_BIOSOURCE(Iter, Bsrc) \
NCBI_NC_ITERATE( \
    (Bsrc).IsSetSubtype(), \
    CBioSource::TSubtype, \
    Iter, \
    (Bsrc).SetSubtype())


/// IF_BIOSOURCE_HAS_ORGMOD

#define IF_BIOSOURCE_HAS_ORGMOD(Bsrc) \
if ((Bsrc).IsSetOrgMod())

/// BIOSOURCE_HAS_ORGMOD

#define BIOSOURCE_HAS_ORGMOD(Bsrc) \
((Bsrc).IsSetOrgMod())

/// FOR_EACH_ORGMOD_ON_BIOSOURCE
// Takes const CBioSource& as input and makes iterator to const COrgMod&
// Dereference with const COrgMod& omd = **iter

#define FOR_EACH_ORGMOD_ON_BIOSOURCE(Iter, Bsrc) \
NCBI_CS_ITERATE( \
    (Bsrc).IsSetOrgMod(), \
    COrgName::TMod, \
    Iter, \
    (Bsrc).GetOrgname().GetMod())

/// EDIT_EACH_ORGMOD_ON_BIOSOURCE
// Takes const CBioSource& as input and makes iterator to const COrgMod&
// Dereference with COrgMod& omd = **iter

#define EDIT_EACH_ORGMOD_ON_BIOSOURCE(Iter, Bsrc) \
NCBI_NC_ITERATE( \
    (Bsrc).IsSetOrgMod(), \
    COrgName::TMod, \
    Iter, \
    (Bsrc).SetOrgname().SetMod())


/// COrgRef iterators

/// IF_ORGREF_HAS_ORGMOD

#define IF_ORGREF_HAS_ORGMOD(Org) \
if ((Org).IsSetOrgMod())

/// ORGREF_HAS_ORGMOD

#define ORGREF_HAS_ORGMOD(Org) \
((Org).IsSetOrgMod())

/// FOR_EACH_ORGMOD_ON_ORGREF
// Takes const COrgRef& as input and makes iterator to const COrgMod&
// Dereference with const COrgMod& omd = **iter

#define FOR_EACH_ORGMOD_ON_ORGREF(Iter, Org) \
NCBI_CS_ITERATE( \
    (Org).IsSetOrgMod(), \
    COrgName::TMod, \
    Iter, \
    (Org).GetOrgname().GetMod())

/// EDIT_EACH_ORGMOD_ON_ORGREF
// Takes const COrgRef& as input and makes iterator to const COrgMod&
// Dereference with COrgMod& omd = **iter

#define EDIT_EACH_ORGMOD_ON_ORGREF(Iter, Org) \
NCBI_NC_ITERATE( \
    (Org).IsSetOrgMod(), \
    COrgName::TMod, \
    Iter, \
    (Org).SetOrgname().SetMod())

/// IF_ORGREF_HAS_DBXREF

#define IF_ORGREF_HAS_DBXREF(Org) \
if ((Org).IsSetDb())

/// ORGREF_HAS_DBXREF

#define ORGREF_HAS_DBXREF(Org) \
((Org).IsSetDb())

/// FOR_EACH_DBXREF_ON_ORGREF
// Takes const COrgRef& as input and makes iterator to const COrgMod&
// Dereference with const COrgMod& omd = **iter

#define FOR_EACH_DBXREF_ON_ORGREF(Iter, Org) \
NCBI_CS_ITERATE( \
    (Org).IsSetDb(), \
    COrg_ref::TDb, \
    Iter, \
    (Org).GetDb())

/// EDIT_EACH_ORGMOD_ON_ORGREF
// Takes const COrgRef& as input and makes iterator to const COrgMod&
// Dereference with COrgMod& omd = **iter

#define EDIT_EACH_DBXREF_ON_ORGREF(Iter, Org) \
NCBI_NC_ITERATE( \
    (Org).IsSetDb(), \
    COrg_ref::TDb, \
    Iter, \
    (Org).SetDb())


/// COrgName iterators

/// IF_ORGNAME_HAS_ORGMOD

#define IF_ORGNAME_HAS_ORGMOD(Onm) \
if ((Onm).IsSetMod())

/// ORGNAME_HAS_ORGMOD

#define ORGNAME_HAS_ORGMOD(Onm) \
((Onm).IsSetMod())

/// FOR_EACH_ORGMOD_ON_ORGNAME
// Takes const COrgName& as input and makes iterator to const COrgMod&
// Dereference with const COrgMod& omd = **iter

#define FOR_EACH_ORGMOD_ON_ORGNAME(Iter, Onm) \
NCBI_CS_ITERATE( \
    (Onm).IsSetMod(), \
    COrgName::TMod, \
    Iter, \
    (Onm).GetMod())

/// EDIT_EACH_ORGMOD_ON_ORGNAME
// Takes const COrgName& as input and makes iterator to const COrgMod&
// Dereference with COrgMod& omd = **iter

#define EDIT_EACH_ORGMOD_ON_ORGNAME(Iter, Onm) \
NCBI_NC_ITERATE( \
    (Onm).IsSetMod(), \
    COrgName::TMod, \
    Iter, \
    (Onm).SetMod())


/// CSubSource iterators

/// IF_SUBSOURCE_CHOICE_IS

#define IF_SUBSOURCE_CHOICE_IS(Sbs, Chs) \
if ((Sbs).IsSetSubtype() && (Sbs).GetSubtype() == Chs)

/// SUBSOURCE_CHOICE_IS

#define SUBSOURCE_CHOICE_IS(Sbs, Chs) \
((Sbs).IsSetSubtype() && (Sbs).GetSubtype() == Chs)

/// SWITCH_ON_SUBSOURCE_CHOICE

#define SWITCH_ON_SUBSOURCE_CHOICE(Sbs) \
NCBI_SWITCH( \
    (Sbs).IsSetSubtype(), \
    (Sbs).GetSubtype())


/// COrgMod iterators

/// IF_ORGMOD_CHOICE_IS

#define IF_ORGMOD_CHOICE_IS(Omd, Chs) \
if ((Omd).IsSetSubtype() && (Omd).GetSubtype() == Chs)

/// ORGMOD_CHOICE_IS

#define ORGMOD_CHOICE_IS(Omd, Chs) \
((Omd).IsSetSubtype() && (Omd).GetSubtype() == Chs)

/// SWITCH_ON_ORGMOD_CHOICE

#define SWITCH_ON_ORGMOD_CHOICE(Omd) \
NCBI_SWITCH( \
    (Omd).IsSetSubtype(), \
    (Omd).GetSubtype())


/// CPubdesc iterators

/// IF_PUBDESC_HAS_PUB

#define IF_PUBDESC_HAS_PUB(Pbd) \
if ((Pbd).IsSetPub())

/// PUBDESC_HAS_PUB

#define PUBDESC_HAS_PUB(Pbd) \
((Pbd).IsSetPub())

/// FOR_EACH_PUB_ON_PUBDESC
// Takes const CPubdesc& as input and makes iterator to const CPub&
// Dereference with const CPub& pub = **iter;

#define FOR_EACH_PUB_ON_PUBDESC(Iter, Pbd) \
NCBI_CS_ITERATE( \
    (Pbd).IsSetPub() && \
        (Pbd).GetPub().IsSet(), \
    CPub_equiv::Tdata, \
    Iter, \
    (Pbd).GetPub().Get())

/// EDIT_EACH_PUB_ON_PUBDESC
// Takes const CPubdesc& as input and makes iterator to const CPub&
// Dereference with CPub& pub = **iter;

#define EDIT_EACH_PUB_ON_PUBDESC(Iter, Pbd) \
NCBI_NC_ITERATE( \
    (Pbd).IsSetPub() && \
        (Pbd).GetPub().IsSet(), \
    CPub_equiv::Tdata, \
    Iter, \
    (Pbd).SetPub().Set())


/// CPub iterators

/// IF_PUB_HAS_AUTHOR

#define IF_PUB_HAS_AUTHOR(Pub) \
if ((Pub).IsSetAuthors() && \
    (Pub).GetAuthors().IsSetNames() && \
    (Pub).GetAuthors().GetNames().IsStd())

/// PUB_HAS_AUTHOR

#define PUB_HAS_AUTHOR(Pub) \
((Pub).IsSetAuthors() && \
    (Pub).GetAuthors().IsSetNames() && \
    (Pub).GetAuthors().GetNames().IsStd())

//// FOR_EACH_AUTHOR_ON_PUB
// Takes const CPub& as input and makes iterator to const CAuthor&
// Dereference with const CAuthor& auth = **iter;

#define FOR_EACH_AUTHOR_ON_PUB(Iter, Pub) \
NCBI_CS_ITERATE( \
    (Pub).IsSetAuthors() && \
        (Pub).GetAuthors().IsSetNames() && \
        (Pub).GetAuthors().GetNames().IsStd() , \
    CAuth_list::C_Names::TStd, \
    Iter, \
    (Pub).GetAuthors().GetNames().GetStd())

//// EDIT_EACH_AUTHOR_ON_PUB
// Takes const CPub& as input and makes iterator to const CAuthor&
// Dereference with CAuthor& auth = **iter;

#define EDIT_EACH_AUTHOR_ON_PUB(Iter, Pub) \
NCBI_NC_ITERATE( \
    (Pub).IsSetAuthors() && \
        (Pub).GetAuthors().IsSetNames() && \
        (Pub).GetAuthors().GetNames().IsStd() , \
    CAuth_list::C_Names::TStd, \
    Iter, \
    (Pub).SetAuthors().SetNames().SetStd())


/// CGB_block iterators

/// IF_GENBANKBLOCK_HAS_EXTRAACCN

#define IF_GENBANKBLOCK_HAS_EXTRAACCN(Gbk) \
if ((Gbk).IsSetExtra_accessions())

/// GENBANKBLOCK_HAS_EXTRAACCN

#define GENBANKBLOCK_HAS_EXTRAACCN(Gbk) \
((Gbk).IsSetExtra_accessions())

/// FOR_EACH_EXTRAACCN_ON_GENBANKBLOCK
// Takes const CGB_block& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_EXTRAACCN_ON_GENBANKBLOCK(Iter, Gbk) \
NCBI_CS_ITERATE( \
    (Gbk).IsSetExtra_accessions(), \
    CGB_block::TExtra_accessions, \
    Iter, \
    (Gbk).GetExtra_accessions())

/// EDIT_EACH_EXTRAACCN_ON_GENBANKBLOCK
// Takes const CGB_block& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_EXTRAACCN_ON_GENBANKBLOCK(Iter, Gbk) \
NCBI_NC_ITERATE( \
    (Gbk).IsSetExtra_accessions(), \
    CGB_block::TExtra_accessions, \
    Iter, \
    (Gbk).SetExtra_accessions())


/// IF_GENBANKBLOCK_HAS_KEYWORD

#define IF_GENBANKBLOCK_HAS_KEYWORD(Gbk) \
if ((Gbk).IsSetKeywords())

/// GENBANKBLOCK_HAS_KEYWORD

#define GENBANKBLOCK_HAS_KEYWORD(Gbk) \
(Gbk).IsSetKeywords()

/// FOR_EACH_KEYWORD_ON_GENBANKBLOCK
// Takes const CGB_block& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_KEYWORD_ON_GENBANKBLOCK(Iter, Gbk) \
NCBI_CS_ITERATE( \
    (Gbk).IsSetKeywords(), \
    CGB_block::TKeywords, \
    Iter, \
    (Gbk).GetKeywords())

/// EDIT_EACH_KEYWORD_ON_GENBANKBLOCK
// Takes const CGB_block& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_KEYWORD_ON_GENBANKBLOCK(Iter, Gbk) \
NCBI_NC_ITERATE( \
    (Gbk).IsSetKeywords(), \
    CGB_block::TKeywords, \
    Iter, \
    (Gbk).SetKeywords())


/// CEMBL_block iterators

/// IF_EMBLBLOCK_HAS_EXTRAACCN

#define IF_EMBLBLOCK_HAS_EXTRAACCN(Ebk) \
if ((Ebk).IsSetExtra_acc())

/// EMBLBLOCK_HAS_EXTRAACCN

#define EMBLBLOCK_HAS_EXTRAACCN(Ebk) \
((Ebk).IsSetExtra_acc())

/// FOR_EACH_EXTRAACCN_ON_EMBLBLOCK
// Takes const CEMBL_block& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_EXTRAACCN_ON_EMBLBLOCK(Iter, Ebk) \
NCBI_CS_ITERATE( \
    (Ebk).IsSetExtra_acc(), \
    CEMBL_block::TExtra_acc, \
    Iter, \
    (Ebk).GetExtra_acc())

/// EDIT_EACH_EXTRAACCN_ON_EMBLBLOCK
// Takes const CEMBL_block& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_EXTRAACCN_ON_EMBLBLOCK(Iter, Ebk) \
NCBI_NC_ITERATE( \
    (Ebk).IsSetExtra_acc(), \
    CEMBL_block::TExtra_acc, \
    Iter, \
    (Ebk).SetExtra_acc())


/// IF_EMBLBLOCK_HAS_KEYWORD

#define IF_EMBLBLOCK_HAS_KEYWORD(Ebk) \
if ((Ebk).IsSetKeywords())

/// EMBLBLOCK_HAS_KEYWORD

#define EMBLBLOCK_HAS_KEYWORD(Ebk) \
((Ebk).IsSetKeywords())

/// FOR_EACH_KEYWORD_ON_EMBLBLOCK
// Takes const CEMBL_block& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_KEYWORD_ON_EMBLBLOCK(Iter, Ebk) \
NCBI_CS_ITERATE( \
    (Ebk).IsSetKeywords(), \
    CEMBL_block::TKeywords, \
    Iter, \
    (Ebk).GetKeywords())

/// EDIT_EACH_KEYWORD_ON_EMBLBLOCK
// Takes const CEMBL_block& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_KEYWORD_ON_EMBLBLOCK(Iter, Ebk) \
NCBI_NC_ITERATE( \
    (Ebk).IsSetKeywords(), \
    CEMBL_block::TKeywords, \
    Iter, \
    (Ebk).SetKeywords())


/// CPDB_block iterators

/// IF_PDBBLOCK_HAS_COMPOUND

#define IF_PDBBLOCK_HAS_COMPOUND(Pbk) \
if ((Pbk).IsSetCompound())

/// PDBBLOCK_HAS_COMPOUND

#define PDBBLOCK_HAS_COMPOUND(Pbk) \
((Pbk).IsSetCompound())

/// FOR_EACH_COMPOUND_ON_PDBBLOCK
// Takes const CPDB_block& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_COMPOUND_ON_PDBBLOCK(Iter, Pbk) \
NCBI_CS_ITERATE( \
    (Pbk).IsSetCompound(), \
    CPDB_block::TCompound, \
    Iter, \
    (Pbk).GetCompound())

/// EDIT_EACH_COMPOUND_ON_PDBBLOCK
// Takes const CPDB_block& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_COMPOUND_ON_PDBBLOCK(Iter, Pbk) \
NCBI_NC_ITERATE( \
    (Pbk).IsSetCompound(), \
    CPDB_block::TCompound, \
    Iter, \
    (Pbk).SetCompound())


/// IF_PDBBLOCK_HAS_SOURCE

#define IF_PDBBLOCK_HAS_SOURCE(Pbk) \
if ((Pbk).IsSetSource())

/// PDBBLOCK_HAS_SOURCE

#define PDBBLOCK_HAS_SOURCE(Pbk) \
((Pbk).IsSetSource())

/// FOR_EACH_SOURCE_ON_PDBBLOCK
// Takes const CPDB_block& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_SOURCE_ON_PDBBLOCK(Iter, Pbk) \
NCBI_CS_ITERATE( \
    (Pbk).IsSetSource(), \
    CPDB_block::TSource, \
    Iter, \
    (Pbk).GetSource())

/// EDIT_EACH_SOURCE_ON_PDBBLOCK
// Takes const CPDB_block& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_SOURCE_ON_PDBBLOCK(Iter, Pbk) \
NCBI_NC_ITERATE( \
    (Pbk).IsSetSource(), \
    CPDB_block::TSource, \
    Iter, \
    (Pbk).SetSource())


/// CSeq_feat iterators

/// IF_FEATURE_CHOICE_IS

#define IF_FEATURE_CHOICE_IS(Sft, Chs) \
if ((Sft).IsSetData() && (Sft).GetData().Which() == Chs)

/// FEATURE_CHOICE_IS

#define FEATURE_CHOICE_IS(Sft, Chs) \
((Sft).IsSetData() && (Sft).GetData().Which() == Chs)

/// SWITCH_ON_FEATURE_CHOICE

#define SWITCH_ON_FEATURE_CHOICE(Sft) \
NCBI_SWITCH( \
    (Sft).IsSetData(), \
    (Sft).GetData().Which())


/// IF_FEATURE_HAS_GBQUAL

#define IF_FEATURE_HAS_GBQUAL(Sft) \
if ((Sft).IsSetQual())

/// FEATURE_HAS_GBQUAL

#define FEATURE_HAS_GBQUAL(Sft) \
((Sft).IsSetQual())

/// FOR_EACH_GBQUAL_ON_FEATURE
// Takes const CSeq_feat& as input and makes iterator to const CGb_qual&
// Dereference with const CGb_qual& gbq = **iter;

#define FOR_EACH_GBQUAL_ON_FEATURE(Iter, Sft) \
NCBI_CS_ITERATE( \
    (Sft).IsSetQual(), \
    CSeq_feat::TQual, \
    Iter, \
    (Sft).GetQual())

/// EDIT_EACH_GBQUAL_ON_FEATURE
// Takes const CSeq_feat& as input and makes iterator to const CGb_qual&
// Dereference with CGb_qual& gbq = **iter;

#define EDIT_EACH_GBQUAL_ON_FEATURE(Iter, Sft) \
NCBI_NC_ITERATE( \
    (Sft).IsSetQual(), \
    CSeq_feat::TQual, \
    Iter, \
    (Sft).SetQual())


/// IF_FEATURE_HAS_SEQFEATXREF

#define IF_FEATURE_HAS_SEQFEATXREF(Sft) \
if ((Sft).IsSetXref())

/// FEATURE_HAS_SEQFEATXREF

#define FEATURE_HAS_SEQFEATXREF(Sft) \
((Sft).IsSetXref())

/// FOR_EACH_SEQFEATXREF_ON_FEATURE
// Takes const CSeq_feat& as input and makes iterator to const CSeqFeatXref&
// Dereference with const CSeqFeatXref& sfx = **iter;

#define FOR_EACH_SEQFEATXREF_ON_FEATURE(Iter, Sft) \
NCBI_CS_ITERATE( \
    (Sft).IsSetXref(), \
    CSeq_feat::TXref, \
    Iter, \
    (Sft).GetXref())

/// EDIT_EACH_SEQFEATXREF_ON_FEATURE
// Takes const CSeq_feat& as input and makes iterator to const CSeqFeatXref&
// Dereference with CSeqFeatXref& sfx = **iter;

#define EDIT_EACH_SEQFEATXREF_ON_FEATURE(Iter, Sft) \
NCBI_NC_ITERATE( \
    (Sft).IsSetXref(), \
    CSeq_feat::TXref, \
    Iter, \
    (Sft).SetXref())


/// IF_FEATURE_HAS_DBXREF

#define IF_FEATURE_HAS_DBXREF(Sft) \
if ((Sft).IsSetDbxref())

/// FEATURE_HAS_DBXREF

#define FEATURE_HAS_DBXREF(Sft) \
((Sft).IsSetDbxref())

/// FOR_EACH_DBXREF_ON_FEATURE
// Takes const CSeq_feat& as input and makes iterator to const CDbtag&
// Dereference with const CDbtag& dbt = **iter;

#define FOR_EACH_DBXREF_ON_FEATURE(Iter, Sft) \
NCBI_CS_ITERATE( \
    (Sft).IsSetDbxref(), \
    CSeq_feat::TDbxref, \
    Iter, \
    (Sft).GetDbxref())

/// EDIT_EACH_DBXREF_ON_FEATURE
// Takes const CSeq_feat& as input and makes iterator to const CDbtag&
// Dereference with CDbtag& dbt = **iter;

#define EDIT_EACH_DBXREF_ON_FEATURE(Iter, Sft) \
NCBI_NC_ITERATE( \
    (Sft).IsSetDbxref(), \
    CSeq_feat::TDbxref, \
    Iter, \
    (Sft).SetDbxref())


/// CGene_ref iterators

/// IF_GENE_HAS_SYNONYM

#define IF_GENE_HAS_SYNONYM(Grf) \
if ((Grf).IsSetSyn())

/// GENE_HAS_SYNONYM

#define GENE_HAS_SYNONYM(Grf) \
((Grf).IsSetSyn())

/// FOR_EACH_SYNONYM_ON_GENE
// Takes const CGene_ref& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_SYNONYM_ON_GENE(Iter, Grf) \
NCBI_CS_ITERATE( \
    (Grf).IsSetSyn(), \
    CGene_ref::TSyn, \
    Iter, \
    (Grf).GetSyn())

/// EDIT_EACH_SYNONYM_ON_GENE
// Takes const CGene_ref& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_SYNONYM_ON_GENE(Iter, Grf) \
NCBI_NC_ITERATE( \
    (Grf).IsSetSyn(), \
    CGene_ref::TSyn, \
    Iter, \
    (Grf).SetSyn())


/// CCdregion iterators

/// IF_CDREGION_HAS_CODEBREAK

#define IF_CDREGION_HAS_CODEBREAK(Cdr) \
if ((Cdr).IsSetCode_break())

/// CDREGION_HAS_CODEBREAK

#define CDREGION_HAS_CODEBREAK(Cdr) \
((Cdr).IsSetCode_break())

/// FOR_EACH_CODEBREAK_ON_CDREGION
// Takes const CCdregion& as input and makes iterator to const CCode_break&
// Dereference with const CCode_break& cbk = **iter;

#define FOR_EACH_CODEBREAK_ON_CDREGION(Iter, Cdr) \
NCBI_CS_ITERATE( \
    (Cdr).IsSetCode_break(), \
    CCdregion::TCode_break, \
    Iter, \
    (Cdr).GetCode_break())

/// EDIT_EACH_CODEBREAK_ON_CDREGION
// Takes const CCdregion& as input and makes iterator to const CCode_break&
// Dereference with CCode_break& cbk = **iter;

#define EDIT_EACH_CODEBREAK_ON_CDREGION(Iter, Cdr) \
NCBI_NC_ITERATE( \
    (Cdr).IsSetCode_break(), \
    CCdregion::TCode_break, \
    Iter, \
    (Cdr).SetCode_break())


/// CProt_ref iterators

/// IF_PROT_HAS_NAME

#define IF_PROT_HAS_NAME(Prf) \
if ((Prf).IsSetName())

/// PROT_HAS_NAME

#define PROT_HAS_NAME(Prf) \
((Prf).IsSetName())

/// FOR_EACH_NAME_ON_PROT
// Takes const CProt_ref& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_NAME_ON_PROT(Iter, Prf) \
NCBI_CS_ITERATE( \
    (Prf).IsSetName(), \
    CProt_ref::TName, \
    Iter, \
    (Prf).GetName())

/// EDIT_EACH_NAME_ON_PROT
// Takes const CProt_ref& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_NAME_ON_PROT(Iter, Prf) \
NCBI_NC_ITERATE( \
    (Prf).IsSetName(), \
    CProt_ref::TName, \
    Iter, \
    (Prf).SetName())


/// IF_PROT_HAS_ECNUMBER

#define IF_PROT_HAS_ECNUMBER(Prf) \
if ((Prf).IsSetEc())

/// PROT_HAS_ECNUMBER

#define PROT_HAS_ECNUMBER(Prf) \
((Prf).IsSetEc())

/// FOR_EACH_ECNUMBER_ON_PROT
// Takes const CProt_ref& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_ECNUMBER_ON_PROT(Iter, Prf) \
NCBI_CS_ITERATE( \
    (Prf).IsSetEc(), \
    CProt_ref::TEc, \
    Iter, \
    (Prf).GetEc())

/// EDIT_EACH_ECNUMBER_ON_PROT
// Takes const CProt_ref& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_ECNUMBER_ON_PROT(Iter, Prf) \
NCBI_NC_ITERATE( \
    (Prf).IsSetEc(), \
    CProt_ref::TEc, \
    Iter, \
    (Prf).SetEc())


/// IF_PROT_HAS_ACTIVITY

#define IF_PROT_HAS_ACTIVITY(Prf) \
if ((Prf).IsSetActivity())

/// PROT_HAS_ACTIVITY

#define PROT_HAS_ACTIVITY(Prf) \
((Prf).IsSetActivity())

/// FOR_EACH_ACTIVITY_ON_PROT
// Takes const CProt_ref& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_ACTIVITY_ON_PROT(Iter, Prf) \
NCBI_CS_ITERATE( \
    (Prf).IsSetActivity(), \
    CProt_ref::TActivity, \
    Iter, \
    (Prf).GetActivity())

/// EDIT_EACH_ACTIVITY_ON_PROT
// Takes const CProt_ref& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_ACTIVITY_ON_PROT(Iter, Prf) \
NCBI_NC_ITERATE( \
    (Prf).IsSetActivity(), \
    CProt_ref::TActivity, \
    Iter, \
    (Prf).SetActivity())


/// list <string> iterators

/// IF_LIST_HAS_STRING

#define IF_LIST_HAS_STRING(Lst) \
if (! (Lst).empty())

/// LIST_HAS_STRING

#define LIST_HAS_STRING(Lst) \
(! (Lst).empty())

/// FOR_EACH_STRING_IN_LIST
// Takes const list <string>& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_STRING_IN_LIST(Iter, Lst) \
NCBI_CS_ITERATE( \
    (! (Lst).empty()), \
    list <string>, \
    Iter, \
    (Lst))

/// EDIT_EACH_STRING_IN_LIST
// Takes const list <string>& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_STRING_IN_LIST(Iter, Lst) \
NCBI_NC_ITERATE( \
    (! (Lst).empty()), \
    list <string>, \
    Iter, \
    (Lst))


/// <string> iterators

/// IF_STRING_HAS_CHAR

#define IF_STRING_HAS_CHAR(Str) \
if (! (Str).empty())

/// STRING_HAS_CHAR

#define STRING_HAS_CHAR(Str) \
(! (Str).empty())

/// FOR_EACH_CHAR_IN_STRING
// Takes const string& as input and makes iterator to const char&
// Dereference with const char& ch = *iter;

#define FOR_EACH_CHAR_IN_STRING(Iter, Str) \
NCBI_CS_ITERATE( \
    (! (Str).empty()), \
    string, \
    Iter, \
    (Str))

/// EDIT_EACH_CHAR_IN_STRING
// Takes const string& as input and makes iterator to const char&
// Dereference with char& ch = *iter;

#define EDIT_EACH_CHAR_IN_STRING(Iter, Str) \
NCBI_CS_ITERATE( \
    (! (Str).empty()), \
    string, \
    Iter, \
    (Str))



/// @}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif /* __SEQUENCE_MACROS__HPP__ */

