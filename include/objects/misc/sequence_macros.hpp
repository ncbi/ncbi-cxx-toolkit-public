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


/// @NAME Convenience macros for NCBI objects
/// @{



/////////////////////////////////////////////////////////////////////////////
/// Macros and typedefs for object subtypes
/////////////////////////////////////////////////////////////////////////////


/// CSeq_entry definitions

#define NCBI_SEQENTRY(Type) CSeq_entry::e_##Type
typedef CSeq_entry::E_Choice TSEQENTRY_CHOICE;

//   Seq     Set


/// CSeq_id definitions

#define NCBI_SEQID(Type) CSeq_id::e_##Type
typedef CSeq_id::E_Choice TSEQID_CHOICE;

//   Local       Gibbsq     Gibbmt      Giim
//   Genbank     Embl       Pir         Swissprot
//   Patent      Other      General     Gi
//   Ddbj        Prf        Pdb         Tpg
//   Tpe         Tpd        Gpipe       Named_annot_track

#define NCBI_ACCN(Type) CSeq_id::eAcc_##Type
typedef CSeq_id::EAccessionInfo TACCN_CHOICE;


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


/// CBioSource definitions

#define NCBI_GENOME(Type) CBioSource::eGenome_##Type
typedef CBioSource::TGenome TBIOSOURCE_GENOME;

//   genomic              chloroplast       chromoplast
//   kinetoplast          mitochondrion     plastid
//   macronuclear         extrachrom        plasmid
//   transposon           insertion_seq     cyanelle
//   proviral             virion            nucleomorph
//   apicoplast           leucoplast        proplastid
//   endogenous_virus     hydrogenosome     chromosome
//   chromatophore

#define NCBI_ORIGIN(Type) CBioSource::eOrigin_##Type
typedef CBioSource::TOrigin TBIOSOURCE_ORIGIN;

//   natural       natmut     mut     artificial
//   synthetic     other


/// COrgName definitions

#define NCBI_ORGNAME(Type) COrgName::e_##Type
typedef COrgName::C_Name::E_Choice TORGNAME_CHOICE;

//   Binomial     Virus     Hybrid     Namedhybrid     Partial


/// CSubSource definitions

#define NCBI_SUBSOURCE(Type) CSubSource::eSubtype_##Type
typedef CSubSource::TSubtype TSUBSOURCE_SUBTYPE;

//   chromosome                map                 clone
//   subclone                  haplotype           genotype
//   sex                       cell_line           cell_type
//   tissue_type               clone_lib           dev_stage
//   frequency                 germline            rearranged
//   lab_host                  pop_variant         tissue_lib
//   plasmid_name              transposon_name     insertion_seq_name
//   plastid_name              country             segment
//   endogenous_virus_name     transgenic          environmental_sample
//   isolation_source          lat_lon             collection_date
//   collected_by              identified_by       fwd_primer_seq
//   rev_primer_seq            fwd_primer_name     rev_primer_name
//   metagenomic               mating_type         linkage_group
//   haplogroup                other


/// COrgMod definitions

#define NCBI_ORGMOD(Type) COrgMod::eSubtype_##Type
typedef COrgMod::TSubtype TORGMOD_SUBTYPE;

//   strain                 substrain        type
//   subtype                variety          serotype
//   serogroup              serovar          cultivar
//   pathovar               chemovar         biovar
//   biotype                group            subgroup
//   isolate                common           acronym
//   dosage                 nat_host         sub_species
//   specimen_voucher       authority        forma
//   forma_specialis        ecotype          synonym
//   anamorph               teleomorph       breed
//   gb_acronym             gb_anamorph      gb_synonym
//   culture_collection     bio_material     metagenome_source
//   old_lineage            old_name         other


/// CPub definitions

#define NCBI_PUB(Type) CPub::e_##Type
typedef CPub::E_Choice TPUB_CHOICE;

//   Gen         Sub       Medline     Muid       Article
//   Journal     Book      Proc        Patent     Pat_id
//   Man         Equiv     Pmid


/// CSeq_feat definitions

#define NCBI_SEQFEAT(Type) CSeqFeatData::e_##Type
typedef CSeqFeatData::E_Choice TSEQFEAT_CHOICE;

//   Gene         Org                 Cdregion     Prot
//   Rna          Pub                 Seq          Imp
//   Region       Comment             Bond         Site
//   Rsite        User                Txinit       Num
//   Psec_str     Non_std_residue     Het          Biosrc



/////////////////////////////////////////////////////////////////////////////
/// Macros for obtaining closest specific CSeqdesc applying to a CBioseq
/////////////////////////////////////////////////////////////////////////////


/// IF_EXISTS_CLOSEST_MOLINFO
// CBioseq& as input, dereference with [const] CMolInfo& molinf = (*cref).GetMolinfo();
// If Lvl is not NULL, it must be a pointer to an int

#define IF_EXISTS_CLOSEST_MOLINFO(Cref, Bsq, Lvl) \
if (CConstRef<CSeqdesc> Cref = (Bsq).GetClosestDescriptor (CSeqdesc::e_Molinfo, Lvl))

/// IF_EXISTS_CLOSEST_BIOSOURCE
// CBioseq& as input, dereference with [const] CBioSource& source = (*cref).GetSource();
// If Lvl is not NULL, it must be a pointer to an int

#define IF_EXISTS_CLOSEST_BIOSOURCE(Cref, Bsq, Lvl) \
if (CConstRef<CSeqdesc> Cref = (Bsq).GetClosestDescriptor (CSeqdesc::e_Source, Lvl))

/// IF_EXISTS_CLOSEST_TITLE
// CBioseq& as input, dereference with [const] string& title = (*cref).GetTitle();
// If Lvl is not NULL, it must be a pointer to an int

#define IF_EXISTS_CLOSEST_TITLE(Cref, Bsq, Lvl) \
if (CConstRef<CSeqdesc> Cref = (Bsq).GetClosestDescriptor (CSeqdesc::e_Title, Lvl))

/// IF_EXISTS_CLOSEST_GENBANKBLOCK
// CBioseq& as input, dereference with [const] CGB_block& gbk = (*cref).GetGenbank();
// If Lvl is not NULL, it must be a pointer to an int

#define IF_EXISTS_CLOSEST_GENBANKBLOCK(Cref, Bsq, Lvl) \
if (CConstRef<CSeqdesc> Cref = (Bsq).GetClosestDescriptor (CSeqdesc::e_Genbank, Lvl))

/// IF_EXISTS_CLOSEST_EMBLBLOCK
// CBioseq& as input, dereference with [const] CEMBL_block& ebk = (*cref).GetEmbl();
// If Lvl is not NULL, it must be a pointer to an int

#define IF_EXISTS_CLOSEST_EMBLBLOCK(Cref, Bsq, Lvl) \
if (CConstRef<CSeqdesc> Cref = (Bsq).GetClosestDescriptor (CSeqdesc::e_Embl, Lvl))

/// IF_EXISTS_CLOSEST_PDBBLOCK
// CBioseq& as input, dereference with [const] CPDB_block& pbk = (*cref).GetPdb();
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
// CSeq_entry& as input, dereference with const CSeq_entry& seqentry = *itr;

#define VISIT_ALL_SEQENTRYS_WITHIN_SEQENTRY(Itr, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeq_entry, \
    Itr, \
    (Seq))

/// VISIT_ALL_BIOSEQS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CBioseq& bioseq = *itr;

#define VISIT_ALL_BIOSEQS_WITHIN_SEQENTRY(Itr, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CBioseq, \
    Itr, \
    (Seq))

/// VISIT_ALL_SEQSETS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CBioseq_set& bss = *itr;

#define VISIT_ALL_SEQSETS_WITHIN_SEQENTRY(Itr, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CBioseq_set, \
    Itr, \
    (Seq))

/// VISIT_ALL_DESCRIPTORS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CSeqdesc& desc = *itr;

#define VISIT_ALL_DESCRIPTORS_WITHIN_SEQENTRY(Itr, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeqdesc, \
    Itr, \
    (Seq))

/// VISIT_ALL_ANNOTS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CSeq_annot& annot = *itr;

#define VISIT_ALL_ANNOTS_WITHIN_SEQENTRY(Itr, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeq_annot, \
    Itr, \
    (Seq))

/// VISIT_ALL_FEATURES_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CSeq_feat& feat = *itr;

#define VISIT_ALL_FEATURES_WITHIN_SEQENTRY(Itr, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeq_feat, \
    Itr, \
    (Seq))

/// VISIT_ALL_ALIGNS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CSeq_align& align = *itr;

#define VISIT_ALL_ALIGNS_WITHIN_SEQENTRY(Itr, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeq_align, \
    Itr, \
    (Seq))

/// VISIT_ALL_GRAPHS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CSeq_graph& graph = *itr;

#define VISIT_ALL_GRAPHS_WITHIN_SEQENTRY(Itr, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeq_graph, \
    Itr, \
    (Seq))


/// CBioseq_set explorers

/// VISIT_ALL_SEQENTRYS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CSeq_entry& seqentry = *itr;

#define VISIT_ALL_SEQENTRYS_WITHIN_SEQSET(Itr, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CSeq_entry, \
    Itr, \
    (Bss))

/// VISIT_ALL_BIOSEQS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CBioseq& bioseq = *itr;

#define VISIT_ALL_BIOSEQS_WITHIN_SEQSET(Itr, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CBioseq, \
    Itr, \
    (Bss))

/// VISIT_ALL_SEQSETS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CBioseq_set& bss = *itr;

#define VISIT_ALL_SEQSETS_WITHIN_SEQSET(Itr, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CBioseq_set, \
    Itr, \
    (Bss))

/// VISIT_ALL_DESCRIPTORS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CSeqdesc& desc = *itr;

#define VISIT_ALL_DESCRIPTORS_WITHIN_SEQSET(Itr, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CSeqdesc, \
    Itr, \
    (Bss))

/// VISIT_ALL_ANNOTS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CSeq_annot& annot = *itr;

#define VISIT_ALL_ANNOTS_WITHIN_SEQSET(Itr, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CSeq_annot, \
    Itr, \
    (Bss))

/// VISIT_ALL_FEATURES_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CSeq_feat& feat = *itr;

#define VISIT_ALL_FEATURES_WITHIN_SEQSET(Itr, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CSeq_feat, \
    Itr, \
    (Bss))

/// VISIT_ALL_ALIGNS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CSeq_align& align = *itr;

#define VISIT_ALL_ALIGNS_WITHIN_SEQSET(Itr, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CSeq_align, \
    Itr, \
    (Bss))

/// VISIT_ALL_GRAPHS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CSeq_graph& graph = *itr;

#define VISIT_ALL_GRAPHS_WITHIN_SEQSET(Itr, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CSeq_graph, \
    Itr, \
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


/// FOR_EACH base macro calls NCBI_CS_ITERATE with generated components

#define FOR_EACH(Base, Itr, Var) \
NCBI_CS_ITERATE(Base##_Test(Var), Base##_Type, Itr, Base##_Get(Var))

/// EDIT_EACH base macro calls NCBI_NC_ITERATE with generated components

#define EDIT_EACH(Base, Itr, Var) \
NCBI_NC_ITERATE(Base##_Test(Var), Base##_Type, Itr, Base##_Set(Var))

/// SWITCH_ON base macro calls NCBI_SWITCH with generated components

#define SWITCH_ON(Base, Var) \
NCBI_SWITCH(Base##_Test(Var), Base##_Chs(Var))


// "FOR_EACH_XXX_ON_YYY" does a linear const traversal of STL containers
// "EDIT_EACH_XXX_ON_YYY" does a linear non-const traversal of STL containers

// "SWITCH_ON_XXX_CHOICE" switches on the item subtype

// "ERASE_XXX_ON_YYY" deletes a specified object within an iterator

// Miscellaneous macros for testing objects include
// "IF_XXX_IS_YYY" or "IF_XXX_IS_YYY"
// "XXX_IS_YYY" or "XXX_HAS_YYY"
// "IF_XXX_CHOICE_IS"
// "XXX_CHOICE_IS"


/// CSeq_submit macros

/// FOR_EACH_SEQENTRY_ON_SEQSUBMIT
/// EDIT_EACH_SEQENTRY_ON_SEQSUBMIT
// CSeq_submit& as input, dereference with [const] CSeq_entry& se = **itr;

#define SEQENTRY_ON_SEQSUBMIT_Type      CSeq_submit::TData::TEntrys
#define SEQENTRY_ON_SEQSUBMIT_Test(Var) (Var).IsEntrys()
#define SEQENTRY_ON_SEQSUBMIT_Get(Var)  (Var).GetData().GetEntrys()
#define SEQENTRY_ON_SEQSUBMIT_Set(Var)  (Var).SetData().SetEntrys()

#define FOR_EACH_SEQENTRY_ON_SEQSUBMIT(Itr, Var) \
FOR_EACH (SEQENTRY_ON_SEQSUBMIT, Itr, Var)

#define EDIT_EACH_SEQENTRY_ON_SEQSUBMIT(Itr, Var) \
EDIT_EACH (SEQENTRY_ON_SEQSUBMIT, Itr, Var)


/// ERASE_SEQENTRY_ON_SEQSUBMIT

#define ERASE_SEQENTRY_ON_SEQSUBMIT(Itr, Ss) \
(Ss).SetData().SetEntrys().erase(Itr)


/// CSeq_entry macros

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

#define SEQENTRY_CHOICE_Test(Var) (Var).Which() != CSeq_entry::e_not_set
#define SEQENTRY_CHOICE_Chs(Var)  (Var).Which()

#define SWITCH_ON_SEQENTRY_CHOICE(Var) \
SWITCH_ON (SEQENTRY_CHOICE, Var)


/// IF_SEQENTRY_HAS_DESCRIPTOR

#define IF_SEQENTRY_HAS_DESCRIPTOR(Seq) \
if ((Seq).IsSetDescr())

/// SEQENTRY_HAS_DESCRIPTOR

#define SEQENTRY_HAS_DESCRIPTOR(Seq) \
((Seq).IsSetDescr())


/// FOR_EACH_SEQDESC_ON_SEQENTRY
/// EDIT_EACH_SEQDESC_ON_SEQENTRY
// CSeq_entry& as input, dereference with [const] CSeqdesc& desc = **itr

#define SEQDESC_ON_SEQENTRY_Type      CSeq_descr::Tdata
#define SEQDESC_ON_SEQENTRY_Test(Var) (Var).IsSetDescr()
#define SEQDESC_ON_SEQENTRY_Get(Var)  (Var).GetDescr().Get()
#define SEQDESC_ON_SEQENTRY_Set(Var)  (Var).SetDescr().Set()

#define FOR_EACH_SEQDESC_ON_SEQENTRY(Itr, Var) \
FOR_EACH (SEQDESC_ON_SEQENTRY, Itr, Var)

#define FOR_EACH_DESCRIPTOR_ON_SEQENTRY FOR_EACH_SEQDESC_ON_SEQENTRY

#define EDIT_EACH_SEQDESC_ON_SEQENTRY(Itr, Var) \
EDIT_EACH (SEQDESC_ON_SEQENTRY, Itr, Var)

#define EDIT_EACH_DESCRIPTOR_ON_SEQENTRY EDIT_EACH_SEQDESC_ON_SEQENTRY


/// ERASE_DESCRIPTOR_ON_SEQENTRY

#define ERASE_DESCRIPTOR_ON_SEQENTRY(Itr, Seq) \
(Seq).SetDescr().Set().erase(Itr)


/// IF_SEQENTRY_HAS_ANNOT

#define IF_SEQENTRY_HAS_ANNOT(Seq) \
if ((Seq).IsSetAnnot())

/// SEQENTRY_HAS_ANNOT

#define SEQENTRY_HAS_ANNOT(Seq) \
((Seq).IsSetAnnot())


/// FOR_EACH_SEQANNOT_ON_SEQENTRY
/// EDIT_EACH_SEQANNOT_ON_SEQENTRY
// CSeq_entry& as input, dereference with [const] CSeq_annot& annot = **itr;

#define SEQANNOT_ON_SEQENTRY_Type      CSeq_entry::TAnnot
#define SEQANNOT_ON_SEQENTRY_Test(Var) (Var).IsSetAnnot()
#define SEQANNOT_ON_SEQENTRY_Get(Var)  (Var).GetAnnot()
#define SEQANNOT_ON_SEQENTRY_Set(Var)  (Var).(Seq).SetAnnot()

#define FOR_EACH_SEQANNOT_ON_SEQENTRY(Itr, Var) \
FOR_EACH (SEQANNOT_ON_SEQENTRY, Itr, Var)

#define FOR_EACH_ANNOT_ON_SEQENTRY FOR_EACH_SEQANNOT_ON_SEQENTRY

#define EDIT_EACH_SEQANNOT_ON_SEQENTRY(Itr, Var) \
EDIT_EACH (SEQANNOT_ON_SEQENTRY, Itr, Var)

#define EDIT_EACH_ANNOT_ON_SEQENTRY EDIT_EACH_SEQANNOT_ON_SEQENTRY


/// ERASE_ANNOT_ON_SEQENTRY

#define ERASE_ANNOT_ON_SEQENTRY(Itr, Seq) \
(Seq).SetAnnot().erase(Itr)


/// CBioseq macros

/// IF_BIOSEQ_HAS_DESCRIPTOR

#define IF_BIOSEQ_HAS_DESCRIPTOR(Bsq) \
if ((Bsq).IsSetDescr())

/// BIOSEQ_HAS_DESCRIPTOR

#define BIOSEQ_HAS_DESCRIPTOR(Bsq) \
((Bsq).IsSetDescr())

/// FOR_EACH_SEQDESC_ON_BIOSEQ
/// EDIT_EACH_SEQDESC_ON_BIOSEQ
// CBioseq& as input, dereference with [const] CSeqdesc& desc = **itr

#define SEQDESC_ON_BIOSEQ_Type      CBioseq::TDescr::Tdata
#define SEQDESC_ON_BIOSEQ_Test(Var) (Var).IsSetDescr()
#define SEQDESC_ON_BIOSEQ_Get(Var)  (Var).GetDescr().Get()
#define SEQDESC_ON_BIOSEQ_Set(Var)  (Var).SetDescr().Set()

#define FOR_EACH_SEQDESC_ON_BIOSEQ(Itr, Var) \
FOR_EACH (SEQDESC_ON_BIOSEQ, Itr, Var)

#define FOR_EACH_DESCRIPTOR_ON_BIOSEQ FOR_EACH_SEQDESC_ON_BIOSEQ

#define EDIT_EACH_SEQDESC_ON_BIOSEQ(Itr, Var) \
EDIT_EACH (SEQDESC_ON_BIOSEQ, Itr, Var)

#define EDIT_EACH_DESCRIPTOR_ON_BIOSEQ EDIT_EACH_SEQDESC_ON_BIOSEQ


/// ERASE_DESCRIPTOR_ON_BIOSEQ

#define ERASE_DESCRIPTOR_ON_BIOSEQ(Itr, Bsq) \
(Bsq).SetDescr().Set().erase(Itr)


/// IF_BIOSEQ_HAS_ANNOT

#define IF_BIOSEQ_HAS_ANNOT(Bsq) \
if ((Bsq).IsSetAnnot())

/// BIOSEQ_HAS_ANNOT

#define BIOSEQ_HAS_ANNOT(Bsq) \
((Bsq).IsSetAnnot())


/// FOR_EACH_SEQANNOT_ON_BIOSEQ
/// EDIT_EACH_SEQANNOT_ON_BIOSEQ
// CBioseq& as input, dereference with [const] CSeq_annot& annot = **itr;

#define SEQANNOT_ON_BIOSEQ_Type      CBioseq::TAnnot
#define SEQANNOT_ON_BIOSEQ_Test(Var) (Var).IsSetAnnot()
#define SEQANNOT_ON_BIOSEQ_Get(Var)  (Var).GetAnnot()
#define SEQANNOT_ON_BIOSEQ_Set(Var)  (Var).SetAnnot()

#define FOR_EACH_SEQANNOT_ON_BIOSEQ(Itr, Var) \
FOR_EACH (SEQANNOT_ON_BIOSEQ, Itr, Var)

#define FOR_EACH_ANNOT_ON_BIOSEQ FOR_EACH_SEQANNOT_ON_BIOSEQ

#define EDIT_EACH_SEQANNOT_ON_BIOSEQ(Itr, Var) \
EDIT_EACH (SEQANNOT_ON_BIOSEQ, Itr, Var)

#define EDIT_EACH_ANNOT_ON_BIOSEQ EDIT_EACH_SEQANNOT_ON_BIOSEQ


/// ERASE_ANNOT_ON_BIOSEQ

#define ERASE_ANNOT_ON_BIOSEQ(Itr, Bsq) \
(Bsq).SetAnnot().erase(Itr)


/// IF_BIOSEQ_HAS_SEQID

#define IF_BIOSEQ_HAS_SEQID(Bsq) \
if ((Bsq).IsSetId())

/// BIOSEQ_HAS_SEQID

#define BIOSEQ_HAS_SEQID(Bsq) \
((Bsq).IsSetId())


/// FOR_EACH_SEQID_ON_BIOSEQ
/// EDIT_EACH_SEQID_ON_BIOSEQ
// CBioseq& as input, dereference with [const] CSeq_id& sid = **itr;

#define SEQID_ON_BIOSEQ_Type      CBioseq::TId
#define SEQID_ON_BIOSEQ_Test(Var) (Var).IsSetId()
#define SEQID_ON_BIOSEQ_Get(Var)  (Var).GetId()
#define SEQID_ON_BIOSEQ_Set(Var)  (Var).SetId()

#define FOR_EACH_SEQID_ON_BIOSEQ(Itr, Var) \
FOR_EACH (SEQID_ON_BIOSEQ, Itr, Var)

#define EDIT_EACH_SEQID_ON_BIOSEQ(Itr, Var) \
EDIT_EACH (SEQID_ON_BIOSEQ, Itr, Var)


/// ERASE_SEQID_ON_BIOSEQ

#define ERASE_SEQID_ON_BIOSEQ(Itr, Bsq) \
(Bsq).SetId().erase(Itr)


/// CSeq_id macros

/// IF_SEQID_CHOICE_IS

#define IF_SEQID_CHOICE_IS(Sid, Chs) \
if ((Sid).Which() == Chs)

/// SEQID_CHOICE_IS

#define SEQID_CHOICE_IS(Sid, Chs) \
((Sid).Which() == Chs)

/// SWITCH_ON_SEQID_CHOICE

#define SEQID_CHOICE_Test(Var) (Var).Which() != CSeq_id::e_not_set
#define SEQID_CHOICE_Chs(Var)  (Var).Which()

#define SWITCH_ON_SEQID_CHOICE(Var) \
SWITCH_ON (SEQID_CHOICE, Var)


/// CBioseq_set macros

/// IF_SEQSET_HAS_DESCRIPTOR

#define IF_SEQSET_HAS_DESCRIPTOR(Bss) \
if ((Bss).IsSetDescr())

/// SEQSET_HAS_DESCRIPTOR

#define SEQSET_HAS_DESCRIPTOR(Bss) \
((Bss).IsSetDescr())


/// FOR_EACH_SEQDESC_ON_SEQSET
/// EDIT_EACH_SEQDESC_ON_SEQSET
// CBioseq_set& as input, dereference with [const] CSeqdesc& desc = **itr;

#define SEQDESC_ON_SEQSET_Type      CBioseq_set::TDescr::Tdata
#define SEQDESC_ON_SEQSET_Test(Var) (Var).IsSetDescr()
#define SEQDESC_ON_SEQSET_Get(Var)  (Var).GetDescr().Get()
#define SEQDESC_ON_SEQSET_Set(Var)  (Var).SetDescr().Set()

#define FOR_EACH_SEQDESC_ON_SEQSET(Itr, Var) \
FOR_EACH (SEQDESC_ON_SEQSET, Itr, Var)

#define FOR_EACH_DESCRIPTOR_ON_SEQSET FOR_EACH_SEQDESC_ON_SEQSET

#define EDIT_EACH_SEQDESC_ON_SEQSET(Itr, Var) \
EDIT_EACH (SEQDESC_ON_SEQSET, Itr, Var)

#define EDIT_EACH_DESCRIPTOR_ON_SEQSET EDIT_EACH_SEQDESC_ON_SEQSET


/// ERASE_DESCRIPTOR_ON_SEQSET

#define ERASE_DESCRIPTOR_ON_SEQSET(Itr, Bss) \
(Bss).SetDescr().Set().erase(Itr)


/// IF_SEQSET_HAS_ANNOT

#define IF_SEQSET_HAS_ANNOT(Bss) \
if ((Bss).IsSetAnnot())

/// SEQSET_HAS_ANNOT

#define SEQSET_HAS_ANNOT(Bss) \
((Bss).IsSetAnnot())


/// FOR_EACH_SEQANNOT_ON_SEQSET
/// EDIT_EACH_SEQANNOT_ON_SEQSET
// CBioseq_set& as input, dereference with [const] CSeq_annot& annot = **itr;

#define SEQANNOT_ON_SEQSET_Type      CBioseq_set::TAnnot
#define SEQANNOT_ON_SEQSET_Test(Var) (Var).IsSetAnnot()
#define SEQANNOT_ON_SEQSET_Get(Var)  (Var).GetAnnot()
#define SEQANNOT_ON_SEQSET_Set(Var)  (Var).SetAnnot()

#define FOR_EACH_SEQANNOT_ON_SEQSET(Itr, Var) \
FOR_EACH (SEQANNOT_ON_SEQSET, Itr, Var)

#define FOR_EACH_ANNOT_ON_SEQSET FOR_EACH_SEQANNOT_ON_SEQSET

#define EDIT_EACH_SEQANNOT_ON_SEQSET(Itr, Var) \
EDIT_EACH (SEQANNOT_ON_SEQSET, Itr, Var)

#define EDIT_EACH_ANNOT_ON_SEQSET EDIT_EACH_SEQANNOT_ON_SEQSET


/// ERASE_ANNOT_ON_SEQSET

#define ERASE_ANNOT_ON_SEQSET(Itr, Bss) \
(Bss).SetAnnot().erase(Itr)


/// IF_SEQSET_HAS_SEQENTRY

#define IF_SEQSET_HAS_SEQENTRY(Bss) \
if ((Bss).IsSetSeq_set())

///SEQSET_HAS_SEQENTRY

#define SEQSET_HAS_SEQENTRY(Bss) \
((Bss).IsSetSeq_set())


/// FOR_EACH_SEQENTRY_ON_SEQSET
/// EDIT_EACH_SEQENTRY_ON_SEQSET
// CBioseq_set& as input, dereference with [const] CSeq_entry& se = **itr;

#define SEQENTRY_ON_SEQSET_Type      CBioseq_set::TSeq_set
#define SEQENTRY_ON_SEQSET_Test(Var) (Var).IsSetSeq_set()
#define SEQENTRY_ON_SEQSET_Get(Var)  (Var).GetSeq_set()
#define SEQENTRY_ON_SEQSET_Set(Var)  (Var).SetSeq_set()

#define FOR_EACH_SEQENTRY_ON_SEQSET(Itr, Var) \
FOR_EACH (SEQENTRY_ON_SEQSET, Itr, Var)

#define EDIT_EACH_SEQENTRY_ON_SEQSET(Itr, Var) \
EDIT_EACH (SEQENTRY_ON_SEQSET, Itr, Var)


/// ERASE_SEQENTRY_ON_SEQSET

#define ERASE_SEQENTRY_ON_SEQSET(Itr, Bss) \
(Bss).SetSeq_set().erase(Itr)


/// CSeq_annot macros

/// IF_ANNOT_IS_FEATURE

#define IF_ANNOT_IS_FEATURE(San) \
if ((San).IsFtable())

/// ANNOT_IS_FEATURE

#define ANNOT_IS_FEATURE(San) \
if ((San).IsFtable())


/// FOR_EACH_SEQFEAT_ON_SEQANNOT
/// EDIT_EACH_SEQFEAT_ON_SEQANNOT
// CSeq_annot& as input, dereference with [const] CSeq_feat& feat = **itr;

#define SEQFEAT_ON_SEQANNOT_Type      CSeq_annot::TData::TFtable
#define SEQFEAT_ON_SEQANNOT_Test(Var) (Var).IsFtable()
#define SEQFEAT_ON_SEQANNOT_Get(Var)  (Var).GetData().GetFtable()
#define SEQFEAT_ON_SEQANNOT_Set(Var)  (Var).SetData().SetFtable()

#define FOR_EACH_SEQFEAT_ON_SEQANNOT(Itr, Var) \
FOR_EACH (SEQFEAT_ON_SEQANNOT, Itr, Var)

#define FOR_EACH_FEATURE_ON_ANNOT FOR_EACH_SEQFEAT_ON_SEQANNOT

#define EDIT_EACH_SEQFEAT_ON_SEQANNOT(Itr, Var) \
EDIT_EACH (SEQFEAT_ON_SEQANNOT, Itr, Var)

#define EDIT_EACH_FEATURE_ON_ANNOT EDIT_EACH_SEQFEAT_ON_SEQANNOT


/// ERASE_FEATURE_ON_ANNOT

#define ERASE_FEATURE_ON_ANNOT(Itr, San) \
(San).SetData().SetFtable().erase(Itr)


/// IF_ANNOT_IS_ALIGN

#define IF_ANNOT_IS_ALIGN(San) \
if ((San).IsAlign())

/// ANNOT_IS_ALIGN

#define ANNOT_IS_ALIGN(San) \
if ((San).IsAlign())


/// FOR_EACH_SEQALIGN_ON_SEQANNOT
/// EDIT_EACH_SEQALIGN_ON_SEQANNOT
// CSeq_annot& as input, dereference with [const] CSeq_align& align = **itr;

#define SEQALIGN_ON_SEQANNOT_Type      CSeq_annot::TData::TAlign
#define SEQALIGN_ON_SEQANNOT_Test(Var) (Var).IsAlign()
#define SEQALIGN_ON_SEQANNOT_Get(Var)  (Var).GetData().GetAlign()
#define SEQALIGN_ON_SEQANNOT_Set(Var)  (Var).SetData().SetAlign()

#define FOR_EACH_SEQALIGN_ON_SEQANNOT(Itr, Var) \
FOR_EACH (SEQALIGN_ON_SEQANNOT, Itr, Var)

#define FOR_EACH_ALIGN_ON_ANNOT FOR_EACH_SEQALIGN_ON_SEQANNOT

#define EDIT_EACH_SEQALIGN_ON_SEQANNOT(Itr, Var) \
EDIT_EACH (SEQALIGN_ON_SEQANNOT, Itr, Var)

#define EDIT_EACH_ALIGN_ON_ANNOT EDIT_EACH_SEQALIGN_ON_SEQANNOT


/// ERASE_ALIGN_ON_ANNOT

#define ERASE_ALIGN_ON_ANNOT(Itr, San) \
(San).SetData().SetAlign().erase(Itr)


/// IF_ANNOT_IS_GRAPH

#define IF_ANNOT_IS_GRAPH(San) \
if ((San).IsGraph())

/// ANNOT_IS_GRAPHS

#define ANNOT_IS_GRAPH(San) \
if ((San).IsGraph())


/// FOR_EACH_SEQGRAPH_ON_SEQANNOT
/// EDIT_EACH_SEQGRAPH_ON_SEQANNOT
// CSeq_annot& as input, dereference with [const] CSeq_graph& graph = **itr;

#define SEQGRAPH_ON_SEQANNOT_Type      CSeq_annot::TData::TGraph
#define SEQGRAPH_ON_SEQANNOT_Test(Var) (Var).IsGraph()
#define SEQGRAPH_ON_SEQANNOT_Get(Var)  (Var).GetData().GetGraph()
#define SEQGRAPH_ON_SEQANNOT_Set(Var)  SetData().SetGraph()

#define FOR_EACH_SEQGRAPH_ON_SEQANNOT(Itr, Var) \
FOR_EACH (SEQGRAPH_ON_SEQANNOT, Itr, Var)

#define FOR_EACH_GRAPH_ON_ANNOT FOR_EACH_SEQGRAPH_ON_SEQANNOT

#define EDIT_EACH_SEQGRAPH_ON_SEQANNOT(Itr, Var) \
EDIT_EACH (SEQGRAPH_ON_SEQANNOT, Itr, Var)

#define EDIT_EACH_GRAPH_ON_ANNOT EDIT_EACH_SEQGRAPH_ON_SEQANNOT


/// ERASE_GRAPH_ON_ANNOT

#define ERASE_GRAPH_ON_ANNOT(Itr, San) \
(San).SetData().SetGraph().erase(Itr)


/// IF_ANNOT_IS_SEQTABLE

#define IF_ANNOT_IS_SEQTABLE(San) \
if ((San).IsSeq_table())

/// ANNOT_IS_SEQTABLE

#define ANNOT_IS_SEQTABLE(San) \
((San).IsSeq_table())


/// IF_ANNOT_HAS_ANNOTDESC

#define IF_ANNOT_HAS_ANNOTDESC(San) \
if ((San).IsSetDesc() && (San).GetDesc().IsSet())

/// ANNOT_HAS_ANNOTDESC

#define ANNOT_HAS_ANNOTDESC(San) \
((San).IsSetDesc() && (San).GetDesc().IsSet())


/// FOR_EACH_ANNOTDESC_ON_SEQANNOT
/// EDIT_EACH_ANNOTDESC_ON_SEQANNOT
// CSeq_annot& as input, dereference with [const] CAnnotdesc& desc = **itr;

#define ANNOTDESC_ON_SEQANNOT_Type      CSeq_annot::TDesc::Tdata
#define ANNOTDESC_ON_SEQANNOT_Test(Var) (Var).IsSetDesc() && (Var).GetDesc().IsSet()
#define ANNOTDESC_ON_SEQANNOT_Get(Var)  (Var).GetDesc().Get()
#define ANNOTDESC_ON_SEQANNOT_Set(Var)  (Var).SetDesc()

#define FOR_EACH_ANNOTDESC_ON_SEQANNOT(Itr, Var) \
FOR_EACH (ANNOTDESC_ON_SEQANNOT, Itr, Var)

#define FOR_EACH_ANNOTDESC_ON_ANNOT FOR_EACH_ANNOTDESC_ON_SEQANNOT

#define EDIT_EACH_ANNOTDESC_ON_SEQANNOT(Itr, Var) \
EDIT_EACH (ANNOTDESC_ON_SEQANNOT, Itr, Var)

#define EDIT_EACH_ANNOTDESC_ON_ANNOT EDIT_EACH_ANNOTDESC_ON_SEQANNOT


/// ERASE_ANNOTDESC_ON_ANNOT

#define ERASE_ANNOTDESC_ON_ANNOT(Itr, San) \
(San).SetDesc().erase(Itr)


/// CAnnotdesc macros

/// IF_ANNOTDESC_CHOICE_IS

#define IF_ANNOTDESC_CHOICE_IS(Ads, Chs) \
if ((Ads).Which() == Chs)

/// ANNOTDESC_CHOICE_IS

#define ANNOTDESC_CHOICE_IS(Ads, Chs) \
((Ads).Which() == Chs)

/// SWITCH_ON_ANNOTDESC_CHOICE

#define ANNOTDESC_CHOICE_Test(Var) (Var).Which() != CAnnotdesc::e_not_set
#define ANNOTDESC_CHOICE_Chs(Var)  (Var).Which()

#define SWITCH_ON_ANNOTDESC_CHOICE(Var) \
SWITCH_ON (ANNOTDESC_CHOICE, Var)

/// CSeq_descr macros

/// IF_DESCR_HAS_DESCRIPTOR

#define IF_DESCR_HAS_DESCRIPTOR(Descr) \
if ((Descr).IsSet())

/// DESCR_HAS_DESCRIPTOR

#define DESCR_HAS_DESCRIPTOR(Descr) \
((Descr).IsSet())


/// FOR_EACH_SEQDESC_ON_SEQDESCR
/// EDIT_EACH_SEQDESC_ON_SEQDESCR
// CSeq_descr& as input, dereference with [const] CSeqdesc& desc = **itr;

#define SEQDESC_ON_SEQDESCR_Type      CSeq_descr::Tdata
#define SEQDESC_ON_SEQDESCR_Test(Var) (Var).IsSet()
#define SEQDESC_ON_SEQDESCR_Get(Var)  (Var).Get()
#define SEQDESC_ON_SEQDESCR_Set(Var)  (Var).Set()

#define FOR_EACH_SEQDESC_ON_SEQDESCR(Itr, Var) \
FOR_EACH (SEQDESC_ON_SEQDESCR, Itr, Var)

#define FOR_EACH_DESCRIPTOR_ON_DESCR FOR_EACH_SEQDESC_ON_SEQDESCR

#define EDIT_EACH_SEQDESC_ON_SEQDESCR(Itr, Var) \
EDIT_EACH (SEQDESC_ON_SEQDESCR, Itr, Var)

#define EDIT_EACH_DESCRIPTOR_ON_DESCR EDIT_EACH_SEQDESC_ON_SEQDESCR


/// ERASE_DESCRIPTOR_ON_DESCR

#define ERASE_DESCRIPTOR_ON_DESCR(Itr, Descr) \
(Descr).Set().erase(Itr)


/// CSeqdesc macros

/// IF_DESCRIPTOR_CHOICE_IS

#define IF_DESCRIPTOR_CHOICE_IS(Dsc, Chs) \
if ((Dsc).Which() == Chs)

/// DESCRIPTOR_CHOICE_IS

#define DESCRIPTOR_CHOICE_IS(Dsc, Chs) \
((Dsc).Which() == Chs)

/// SWITCH_ON_SEQDESC_CHOICE

#define SEQDESC_CHOICE_Test(Var) (Var).Which() != CSeqdesc::e_not_set
#define SEQDESC_CHOICE_Chs(Var)  (Var).Which()

#define SWITCH_ON_SEQDESC_CHOICE(Var) \
SWITCH_ON (SEQDESC_CHOICE, Var)

#define SWITCH_ON_DESCRIPTOR_CHOICE SWITCH_ON_SEQDESC_CHOICE


/// CMolInfo macros

/// IF_MOLINFO_BIOMOL_IS

#define IF_MOLINFO_BIOMOL_IS(Mif, Chs) \
if ((Mif).IsSetBiomol() && (Mif).GetBiomol() == Chs)

/// MOLINFO_BIOMOL_IS

#define MOLINFO_BIOMOL_IS(Mif, Chs) \
((Mif).IsSetBiomol() && (Mif).GetBiomol() == Chs)


/// SWITCH_ON_MOLINFO_BIOMOL

#define MOLINFO_BIOMOL_Test(Var) (Var).IsSetBiomol()
#define MOLINFO_BIOMOL_Chs(Var)  (Var).GetBiomol()

#define SWITCH_ON_MOLINFO_BIOMOL(Var) \
SWITCH_ON (MOLINFO_BIOMOL, Var)

/// IF_MOLINFO_TECH_IS

#define IF_MOLINFO_TECH_IS(Mif, Chs) \
if ((Mif).IsSetTech() && (Mif).GetTech() == Chs)

/// MOLINFO_TECH_IS

#define MOLINFO_TECH_IS(Mif, Chs) \
((Mif).IsSetTech() && (Mif).GetTech() == Chs)

/// SWITCH_ON_MOLINFO_TECH

#define MOLINFO_TECH_Test(Var) (Var).IsSetTech()
#define MOLINFO_TECH_Chs(Var)  (Var).GetTech()

#define SWITCH_ON_MOLINFO_TECH(Var) \
SWITCH_ON (MOLINFO_TECH, Var)


/// IF_MOLINFO_COMPLETENESS_IS

#define IF_MOLINFO_COMPLETENESS_IS(Mif, Chs) \
if ((Mif).IsSetCompleteness() && (Mif).GetCompleteness() == Chs)

/// MOLINFO_COMPLETENESS_IS

#define MOLINFO_COMPLETENESS_IS(Mif, Chs) \
((Mif).IsSetCompleteness() && (Mif).GetCompleteness() == Chs)

/// SWITCH_ON_MOLINFO_COMPLETENESS

#define MOLINFO_COMPLETENESS_Test(Var) (Var).IsSetCompleteness()
#define MOLINFO_COMPLETENESS_Chs(Var)  (Var).GetCompleteness()

#define SWITCH_ON_MOLINFO_COMPLETENESS(Var) \
SWITCH_ON (MOLINFO_COMPLETENESS, Var)


/// CBioSource macros

/// IF_BIOSOURCE_GENOME_IS

#define IF_BIOSOURCE_GENOME_IS(Bsc, Chs) \
if ((Bsc).IsSetGenome() && (Bsc).GetGenome() == Chs)

/// BIOSOURCE_GENOME_IS

#define BIOSOURCE_GENOME_IS(Bsc, Chs) \
((Bsc).IsSetGenome() && (Bsc).GetGenome() == Chs)

/// SWITCH_ON_BIOSOURCE_GENOME

#define BIOSOURCE_GENOME_Test(Var) (Var).IsSetGenome()
#define BIOSOURCE_GENOME_Chs(Var)  (Var).GetGenome()

#define SWITCH_ON_BIOSOURCE_GENOME(Var) \
SWITCH_ON (BIOSOURCE_GENOME, Var)


/// IF_BIOSOURCE_ORIGIN_IS

#define IF_BIOSOURCE_ORIGIN_IS(Bsc, Chs) \
if ((Bsc).IsSetOrigin() && (Bsc).GetOrigin() == Chs)

/// BIOSOURCE_ORIGIN_IS

#define BIOSOURCE_ORIGIN_IS(Bsc, Chs) \
((Bsc).IsSetOrigin() && (Bsc).GetOrigin() == Chs)

/// SWITCH_ON_BIOSOURCE_ORIGIN

#define BIOSOURCE_ORIGIN_Test(Var) (Var).IsSetOrigin()
#define BIOSOURCE_ORIGIN_Chs(Var)  (Var).GetOrigin()

#define SWITCH_ON_BIOSOURCE_ORIGIN(Var) \
SWITCH_ON (BIOSOURCE_ORIGIN, Var)


/// IF_BIOSOURCE_HAS_ORGREF

#define IF_BIOSOURCE_HAS_ORGREF(Bsc) \
if ((Bsc).IsSetOrg())

/// BIOSOURCE_HAS_ORGREF

#define BIOSOURCE_HAS_ORGREF(Bsc) \
((Bsc).IsSetOrg())


/// IF_BIOSOURCE_HAS_ORGNAME

#define IF_BIOSOURCE_HAS_ORGNAME(Bsc) \
if ((Bsc).IsSetOrgname())

/// BIOSOURCE_HAS_ORGNAME

#define BIOSOURCE_HAS_ORGNAME(Bsc) \
((Bsc).IsSetOrgname())


/// IF_BIOSOURCE_HAS_SUBSOURCE

#define IF_BIOSOURCE_HAS_SUBSOURCE(Bsc) \
if ((Bsc).IsSetSubtype())

/// BIOSOURCE_HAS_SUBSOURCE

#define BIOSOURCE_HAS_SUBSOURCE(Bsc) \
((Bsc).IsSetSubtype())


/// FOR_EACH_SUBSOURCE_ON_BIOSOURCE
/// EDIT_EACH_SUBSOURCE_ON_BIOSOURCE
// CBioSource& as input, dereference with [const] CSubSource& sbs = **itr

#define SUBSOURCE_ON_BIOSOURCE_Type      CBioSource::TSubtype
#define SUBSOURCE_ON_BIOSOURCE_Test(Var) (Var).IsSetSubtype()
#define SUBSOURCE_ON_BIOSOURCE_Get(Var)  (Var).GetSubtype()
#define SUBSOURCE_ON_BIOSOURCE_Set(Var)  (Var).SetSubtype()

#define FOR_EACH_SUBSOURCE_ON_BIOSOURCE(Itr, Var) \
FOR_EACH (SUBSOURCE_ON_BIOSOURCE, Itr, Var)

#define EDIT_EACH_SUBSOURCE_ON_BIOSOURCE(Itr, Var) \
EDIT_EACH (SUBSOURCE_ON_BIOSOURCE, Itr, Var)


/// ERASE_SUBSOURCE_ON_BIOSOURCE

#define ERASE_SUBSOURCE_ON_BIOSOURCE(Itr, Bsc) \
(Bsc).SetSubtype().erase(Itr)


/// IF_BIOSOURCE_HAS_ORGMOD

#define IF_BIOSOURCE_HAS_ORGMOD(Bsc) \
if ((Bsc).IsSetOrgMod())

/// BIOSOURCE_HAS_ORGMOD

#define BIOSOURCE_HAS_ORGMOD(Bsc) \
((Bsc).IsSetOrgMod())


/// FOR_EACH_ORGMOD_ON_BIOSOURCE
/// EDIT_EACH_ORGMOD_ON_BIOSOURCE
// CBioSource& as input, dereference with [const] COrgMod& omd = **itr

#define ORGMOD_ON_BIOSOURCE_Type      COrgName::TMod
#define ORGMOD_ON_BIOSOURCE_Test(Var) (Var).IsSetOrgMod()
#define ORGMOD_ON_BIOSOURCE_Get(Var)  (Var).GetOrgname().GetMod()
#define ORGMOD_ON_BIOSOURCE_Set(Var)  (Var).SetOrgname().SetMod()

#define FOR_EACH_ORGMOD_ON_BIOSOURCE(Itr, Var) \
FOR_EACH (ORGMOD_ON_BIOSOURCE, Itr, Var)

#define EDIT_EACH_ORGMOD_ON_BIOSOURCE(Itr, Var) \
EDIT_EACH (ORGMOD_ON_BIOSOURCE, Itr, Var)


/// ERASE_ORGMOD_ON_BIOSOURCE

#define ERASE_ORGMOD_ON_BIOSOURCE(Itr, Bsc) \
(Bsc).SetOrgname().SetMod().erase(Itr)


/// COrg_ref macros

/// IF_ORGREF_HAS_ORGMOD

#define IF_ORGREF_HAS_ORGMOD(Org) \
if ((Org).IsSetOrgMod())

/// ORGREF_HAS_ORGMOD

#define ORGREF_HAS_ORGMOD(Org) \
((Org).IsSetOrgMod())


/// FOR_EACH_ORGMOD_ON_ORGREF
/// EDIT_EACH_ORGMOD_ON_ORGREF
// COrg_ref& as input, dereference with [const] COrgMod& omd = **itr

#define ORGMOD_ON_ORGREF_Type      COrgName::TMod
#define ORGMOD_ON_ORGREF_Test(Var) (Var).IsSetOrgMod()
#define ORGMOD_ON_ORGREF_Get(Var)  (Var).GetOrgname().GetMod()
#define ORGMOD_ON_ORGREF_Set(Var)  (Var).SetOrgname().SetMod()

#define FOR_EACH_ORGMOD_ON_ORGREF(Itr, Var) \
FOR_EACH (ORGMOD_ON_ORGREF, Itr, Var)

#define EDIT_EACH_ORGMOD_ON_ORGREF(Itr, Var) \
EDIT_EACH (ORGMOD_ON_ORGREF, Itr, Var)


/// ERASE_ORGMOD_ON_ORGREF

#define ERASE_ORGMOD_ON_ORGREF(Itr, Org) \
(Org).SetOrgname().SetMod().erase(Itr)


/// IF_ORGREF_HAS_DBXREF

#define IF_ORGREF_HAS_DBXREF(Org) \
if ((Org).IsSetDb())

/// ORGREF_HAS_DBXREF

#define ORGREF_HAS_DBXREF(Org) \
((Org).IsSetDb())


/// FOR_EACH_DBXREF_ON_ORGREF
/// EDIT_EACH_DBXREF_ON_ORGREF
// COrg_ref& as input, dereference with [const] CDbtag& dbt = **itr

#define DBXREF_ON_ORGREF_Type      COrg_ref::TDb
#define DBXREF_ON_ORGREF_Test(Var) (Var).IsSetDb()
#define DBXREF_ON_ORGREF_Get(Var)  (Var).GetDb()
#define DBXREF_ON_ORGREF_Set(Var)  (Var).SetDb()

#define FOR_EACH_DBXREF_ON_ORGREF(Itr, Var) \
FOR_EACH (DBXREF_ON_ORGREF, Itr, Var)

#define EDIT_EACH_DBXREF_ON_ORGREF(Itr, Var) \
EDIT_EACH (DBXREF_ON_ORGREF, Itr, Var)


/// ERASE_DBXREF_ON_ORGREF

#define ERASE_DBXREF_ON_ORGREF(Itr, Org) \
(Org).SetDb().erase(Itr)


/// IF_ORGREF_HAS_MOD

#define IF_ORGREF_HAS_MOD(Org) \
if ((Org).IsSetMod())

/// ORGREF_HAS_MOD

#define ORGREF_HAS_MOD(Org) \
((Org).IsSetMod())


/// FOR_EACH_MOD_ON_ORGREF
/// EDIT_EACH_MOD_ON_ORGREF
// COrg_ref& as input, dereference with [const] string& str = *itr

#define MOD_ON_ORGREF_Type      COrg_ref::TMod
#define MOD_ON_ORGREF_Test(Var) (Var).IsSetMod()
#define MOD_ON_ORGREF_Get(Var)  (Var).GetMod()
#define MOD_ON_ORGREF_Set(Var)  (Var).SetMod()

#define FOR_EACH_MOD_ON_ORGREF(Itr, Var) \
FOR_EACH (MOD_ON_ORGREF, Itr, Var)

#define EDIT_EACH_MOD_ON_ORGREF(Itr, Var) \
EDIT_EACH (MOD_ON_ORGREF, Itr, Var)


/// ERASE_MOD_ON_ORGREF

#define ERASE_MOD_ON_ORGREF(Itr, Org) \
(Org).SetMod().erase(Itr)


/// COrgName macros

/// IF_ORGNAME_CHOICE_IS

#define IF_ORGNAME_CHOICE_IS(Onm, Chs) \
if ((Onm).IsSetName() && (Onm).GetName().Which() == Chs)

/// ORGNAME_CHOICE_IS

#define ORGNAME_CHOICE_IS(Onm, Chs) \
((Onm).IsSetName() && (Onm).GetName().Which() == Chs)

/// SWITCH_ON_ORGNAME_CHOICE

#define ORGNAME_CHOICE_Test(Var) (Var).IsSetName() && \
                                     (Var).GetName().Which() != COrgName::e_not_set
#define ORGNAME_CHOICE_Chs(Var)  (Var).GetName().Which()

#define SWITCH_ON_ORGNAME_CHOICE(Var) \
SWITCH_ON (ORGNAME_CHOICE, Var)


/// IF_ORGNAME_HAS_ORGMOD

#define IF_ORGNAME_HAS_ORGMOD(Onm) \
if ((Onm).IsSetMod())

/// ORGNAME_HAS_ORGMOD

#define ORGNAME_HAS_ORGMOD(Onm) \
((Onm).IsSetMod())


/// FOR_EACH_ORGMOD_ON_ORGNAME
/// EDIT_EACH_ORGMOD_ON_ORGNAME
// COrgName& as input, dereference with [const] COrgMod& omd = **itr

#define ORGMOD_ON_ORGNAME_Type      COrgName::TMod
#define ORGMOD_ON_ORGNAME_Test(Var) (Var).IsSetMod()
#define ORGMOD_ON_ORGNAME_Get(Var)  (Var).GetMod()
#define ORGMOD_ON_ORGNAME_Set(Var)  (Var).SetMod()

#define FOR_EACH_ORGMOD_ON_ORGNAME(Itr, Var) \
FOR_EACH (ORGMOD_ON_ORGNAME, Itr, Var)

#define EDIT_EACH_ORGMOD_ON_ORGNAME(Itr, Var) \
EDIT_EACH (ORGMOD_ON_ORGNAME, Itr, Var)


/// ERASE_ORGMOD_ON_ORGNAME

#define ERASE_ORGMOD_ON_ORGNAME(Itr, Onm) \
(Onm).SetMod().erase(Itr)


/// CSubSource macros

/// IF_SUBSOURCE_CHOICE_IS

#define IF_SUBSOURCE_CHOICE_IS(Sbs, Chs) \
if ((Sbs).IsSetSubtype() && (Sbs).GetSubtype() == Chs)

/// SUBSOURCE_CHOICE_IS

#define SUBSOURCE_CHOICE_IS(Sbs, Chs) \
((Sbs).IsSetSubtype() && (Sbs).GetSubtype() == Chs)

/// SWITCH_ON_SUBSOURCE_CHOICE

#define SUBSOURCE_CHOICE_Test(Var) (Var).IsSetSubtype()
#define SUBSOURCE_CHOICE_Chs(Var)  (Var).GetSubtype()

#define SWITCH_ON_SUBSOURCE_CHOICE(Var) \
SWITCH_ON (SUBSOURCE_CHOICE, Var)


/// COrgMod macros

/// IF_ORGMOD_CHOICE_IS

#define IF_ORGMOD_CHOICE_IS(Omd, Chs) \
if ((Omd).IsSetSubtype() && (Omd).GetSubtype() == Chs)

/// ORGMOD_CHOICE_IS

#define ORGMOD_CHOICE_IS(Omd, Chs) \
((Omd).IsSetSubtype() && (Omd).GetSubtype() == Chs)

/// SWITCH_ON_ORGMOD_CHOICE

#define ORGMOD_CHOICE_Test(Var) (Var).IsSetSubtype()
#define ORGMOD_CHOICE_Chs(Var)  (Var).GetSubtype()

#define SWITCH_ON_ORGMOD_CHOICE(Var) \
SWITCH_ON (ORGMOD_CHOICE, Var)


/// CPubdesc macros

/// IF_PUBDESC_HAS_PUB

#define IF_PUBDESC_HAS_PUB(Pbd) \
if ((Pbd).IsSetPub())

/// PUBDESC_HAS_PUB

#define PUBDESC_HAS_PUB(Pbd) \
((Pbd).IsSetPub())


/// FOR_EACH_PUB_ON_PUBDESC
/// EDIT_EACH_PUB_ON_PUBDESC
// CPubdesc& as input, dereference with [const] CPub& pub = **itr;

#define PUB_ON_PUBDESC_Type      CPub_equiv::Tdata
#define PUB_ON_PUBDESC_Test(Var) (Var).IsSetPub() && (Var).GetPub().IsSet()
#define PUB_ON_PUBDESC_Get(Var)  (Var).GetPub().Get()
#define PUB_ON_PUBDESC_Set(Var)  (Var).SetPub().Set()

#define FOR_EACH_PUB_ON_PUBDESC(Itr, Var) \
FOR_EACH (PUB_ON_PUBDESC, Itr, Var)

#define EDIT_EACH_PUB_ON_PUBDESC(Itr, Var) \
EDIT_EACH (PUB_ON_PUBDESC, Itr, Var)


/// ERASE_PUB_ON_PUBDESC

#define ERASE_PUB_ON_PUBDESC(Itr, Pbd) \
(Pbd).SetPub().Set().erase(Itr)


/// CPub macros

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


/// FOR_EACH_AUTHOR_ON_PUB
/// EDIT_EACH_AUTHOR_ON_PUB
// CPub& as input, dereference with [const] CAuthor& auth = **itr;

#define AUTHOR_ON_PUB_Type      CAuth_list::C_Names::TStd
#define AUTHOR_ON_PUB_Test(Var) (Var).IsSetAuthors() && \
                                    (Var).GetAuthors().IsSetNames() && \
                                    (Var).GetAuthors().GetNames().IsStd()
#define AUTHOR_ON_PUB_Get(Var)  (Var).GetAuthors().GetNames().GetStd()
#define AUTHOR_ON_PUB_Set(Var)  (Var).SetAuthors().SetNames().SetStd()

#define FOR_EACH_AUTHOR_ON_PUB(Itr, Var) \
FOR_EACH (AUTHOR_ON_PUB, Itr, Var)

#define EDIT_EACH_AUTHOR_ON_PUB(Itr, Var) \
EDIT_EACH (AUTHOR_ON_PUB, Itr, Var)


/// ERASE_AUTHOR_ON_PUB

#define ERASE_AUTHOR_ON_PUB(Itr, Pub) \
(Pub).SetAuthors().SetNames().SetStd().erase(Itr)


/// CGB_block macros

/// IF_GENBANKBLOCK_HAS_EXTRAACCN

#define IF_GENBANKBLOCK_HAS_EXTRAACCN(Gbk) \
if ((Gbk).IsSetExtra_accessions())

/// GENBANKBLOCK_HAS_EXTRAACCN

#define GENBANKBLOCK_HAS_EXTRAACCN(Gbk) \
((Gbk).IsSetExtra_accessions())


/// FOR_EACH_EXTRAACCN_ON_GENBANKBLOCK
/// EDIT_EACH_EXTRAACCN_ON_GENBANKBLOCK
// CGB_block& as input, dereference with [const] string& str = *itr;

#define EXTRAACCN_ON_GENBANKBLOCK_Type      CGB_block::TExtra_accessions
#define EXTRAACCN_ON_GENBANKBLOCK_Test(Var) (Var).IsSetExtra_accessions()
#define EXTRAACCN_ON_GENBANKBLOCK_Get(Var)  (Var).GetExtra_accessions()
#define EXTRAACCN_ON_GENBANKBLOCK_Set(Var)  (Var).SetExtra_accessions()

#define FOR_EACH_EXTRAACCN_ON_GENBANKBLOCK(Itr, Var) \
FOR_EACH (EXTRAACCN_ON_GENBANKBLOCK, Itr, Var)

#define EDIT_EACH_EXTRAACCN_ON_GENBANKBLOCK(Itr, Var) \
EDIT_EACH (EXTRAACCN_ON_GENBANKBLOCK, Itr, Var)


/// ERASE_EXTRAACCN_ON_GENBANKBLOCK

#define ERASE_EXTRAACCN_ON_GENBANKBLOCK(Itr, Gbk) \
(Gbk).SetExtra_accessions().erase(Itr)


/// IF_GENBANKBLOCK_HAS_KEYWORD

#define IF_GENBANKBLOCK_HAS_KEYWORD(Gbk) \
if ((Gbk).IsSetKeywords())

/// GENBANKBLOCK_HAS_KEYWORD

#define GENBANKBLOCK_HAS_KEYWORD(Gbk) \
(Gbk).IsSetKeywords()


/// FOR_EACH_KEYWORD_ON_GENBANKBLOCK
/// EDIT_EACH_KEYWORD_ON_GENBANKBLOCK
// CGB_block& as input, dereference with [const] string& str = *itr;

#define KEYWORD_ON_GENBANKBLOCK_Type      CGB_block::TKeywords
#define KEYWORD_ON_GENBANKBLOCK_Test(Var) (Var).IsSetKeywords()
#define KEYWORD_ON_GENBANKBLOCK_Get(Var)  (Var).GetKeywords()
#define KEYWORD_ON_GENBANKBLOCK_Set(Var)  (Var).SetKeywords()

#define FOR_EACH_KEYWORD_ON_GENBANKBLOCK(Itr, Var) \
FOR_EACH (KEYWORD_ON_GENBANKBLOCK, Itr, Var)

#define EDIT_EACH_KEYWORD_ON_GENBANKBLOCK(Itr, Var) \
EDIT_EACH (KEYWORD_ON_GENBANKBLOCK, Itr, Var)


/// ERASE_KEYWORD_ON_GENBANKBLOCK

#define ERASE_KEYWORD_ON_GENBANKBLOCK(Itr, Gbk) \
(Gbk).SetKeywords().erase(Itr)


/// CEMBL_block macros

/// IF_EMBLBLOCK_HAS_EXTRAACCN

#define IF_EMBLBLOCK_HAS_EXTRAACCN(Ebk) \
if ((Ebk).IsSetExtra_acc())

/// EMBLBLOCK_HAS_EXTRAACCN

#define EMBLBLOCK_HAS_EXTRAACCN(Ebk) \
((Ebk).IsSetExtra_acc())


/// FOR_EACH_EXTRAACCN_ON_EMBLBLOCK
/// EDIT_EACH_EXTRAACCN_ON_EMBLBLOCK
// CEMBL_block& as input, dereference with [const] string& str = *itr;

#define EXTRAACCN_ON_EMBLBLOCK_Type      CEMBL_block::TExtra_acc
#define EXTRAACCN_ON_EMBLBLOCK_Test(Var) (Var).IsSetExtra_acc()
#define EXTRAACCN_ON_EMBLBLOCK_Get(Var)  (Var).GetExtra_acc()
#define EXTRAACCN_ON_EMBLBLOCK_Set(Var)  (Var).SetExtra_acc()

#define FOR_EACH_EXTRAACCN_ON_EMBLBLOCK(Itr, Var) \
FOR_EACH (EXTRAACCN_ON_EMBLBLOCK, Itr, Var)

#define EDIT_EACH_EXTRAACCN_ON_EMBLBLOCK(Itr, Var) \
EDIT_EACH (EXTRAACCN_ON_EMBLBLOCK, Itr, Var)


/// ERASE_EXTRAACCN_ON_EMBLBLOCK

#define ERASE_EXTRAACCN_ON_EMBLBLOCK(Itr, Ebk) \
(Ebk).SetExtra_acc().erase(Itr)


/// IF_EMBLBLOCK_HAS_KEYWORD

#define IF_EMBLBLOCK_HAS_KEYWORD(Ebk) \
if ((Ebk).IsSetKeywords())

/// EMBLBLOCK_HAS_KEYWORD

#define EMBLBLOCK_HAS_KEYWORD(Ebk) \
((Ebk).IsSetKeywords())


/// FOR_EACH_KEYWORD_ON_EMBLBLOCK
/// EDIT_EACH_KEYWORD_ON_EMBLBLOCK
// CEMBL_block& as input, dereference with [const] string& str = *itr;

#define KEYWORD_ON_EMBLBLOCK_Type      CEMBL_block::TKeywords
#define KEYWORD_ON_EMBLBLOCK_Test(Var) (Var).IsSetKeywords()
#define KEYWORD_ON_EMBLBLOCK_Get(Var)  (Var).GetKeywords()
#define KEYWORD_ON_EMBLBLOCK_Set(Var)  (Var).SetKeywords()

#define FOR_EACH_KEYWORD_ON_EMBLBLOCK(Itr, Var) \
FOR_EACH (KEYWORD_ON_EMBLBLOCK, Itr, Var)

#define EDIT_EACH_KEYWORD_ON_EMBLBLOCK(Itr, Var) \
EDIT_EACH (KEYWORD_ON_EMBLBLOCK, Itr, Var)


/// ERASE_KEYWORD_ON_EMBLBLOCK

#define ERASE_KEYWORD_ON_EMBLBLOCK(Itr, Ebk) \
(Ebk).SetKeywords().erase(Itr)


/// CPDB_block macros

/// IF_PDBBLOCK_HAS_COMPOUND

#define IF_PDBBLOCK_HAS_COMPOUND(Pbk) \
if ((Pbk).IsSetCompound())

/// PDBBLOCK_HAS_COMPOUND

#define PDBBLOCK_HAS_COMPOUND(Pbk) \
((Pbk).IsSetCompound())


/// FOR_EACH_COMPOUND_ON_PDBBLOCK
/// EDIT_EACH_COMPOUND_ON_PDBBLOCK
// CPDB_block& as input, dereference with [const] string& str = *itr;

#define COMPOUND_ON_PDBBLOCK_Type      CPDB_block::TCompound
#define COMPOUND_ON_PDBBLOCK_Test(Var) (Var).IsSetCompound()
#define COMPOUND_ON_PDBBLOCK_Get(Var)  (Var).GetCompound()
#define COMPOUND_ON_PDBBLOCK_Set(Var)  (Var).SetCompound()

#define FOR_EACH_COMPOUND_ON_PDBBLOCK(Itr, Var) \
FOR_EACH (COMPOUND_ON_PDBBLOCK, Itr, Var)

#define EDIT_EACH_COMPOUND_ON_PDBBLOCK(Itr, Var) \
EDIT_EACH (COMPOUND_ON_PDBBLOCK, Itr, Var)


/// ERASE_COMPOUND_ON_PDBBLOCK

#define ERASE_COMPOUND_ON_PDBBLOCK(Itr, Pbk) \
(Pbk).SetCompound().erase(Itr)


/// IF_PDBBLOCK_HAS_SOURCE

#define IF_PDBBLOCK_HAS_SOURCE(Pbk) \
if ((Pbk).IsSetSource())

/// PDBBLOCK_HAS_SOURCE

#define PDBBLOCK_HAS_SOURCE(Pbk) \
((Pbk).IsSetSource())


/// FOR_EACH_SOURCE_ON_PDBBLOCK
/// EDIT_EACH_SOURCE_ON_PDBBLOCK
// CPDB_block& as input, dereference with [const] string& str = *itr;

#define SOURCE_ON_PDBBLOCK_Type      CPDB_block::TSource
#define SOURCE_ON_PDBBLOCK_Test(Var) (Var).IsSetSource()
#define SOURCE_ON_PDBBLOCK_Get(Var)  (Var).GetSource()
#define SOURCE_ON_PDBBLOCK_Set(Var)  (Var).SetSource()

#define FOR_EACH_SOURCE_ON_PDBBLOCK(Itr, Var) \
FOR_EACH (SOURCE_ON_PDBBLOCK, Itr, Var)

#define EDIT_EACH_SOURCE_ON_PDBBLOCK(Itr, Var) \
EDIT_EACH (SOURCE_ON_PDBBLOCK, Itr, Var)


/// ERASE_SOURCE_ON_PDBBLOCK

#define ERASE_SOURCE_ON_PDBBLOCK(Itr, Pbk) \
(Pbk).SetSource().erase(Itr)


/// CSeq_feat macros

/// IF_FEATURE_CHOICE_IS

#define IF_FEATURE_CHOICE_IS(Sft, Chs) \
if ((Sft).IsSetData() && (Sft).GetData().Which() == Chs)

/// FEATURE_CHOICE_IS

#define FEATURE_CHOICE_IS(Sft, Chs) \
((Sft).IsSetData() && (Sft).GetData().Which() == Chs)

/// SWITCH_ON_SEQFEAT_CHOICE

#define SEQFEAT_CHOICE_Test(Var) (Var).IsSetData()
#define SEQFEAT_CHOICE_Chs(Var)  (Var).GetData().Which()

#define SWITCH_ON_SEQFEAT_CHOICE(Var) \
SWITCH_ON (SEQFEAT_CHOICE, Var)

#define SWITCH_ON_FEATURE_CHOICE SWITCH_ON_SEQFEAT_CHOICE


/// IF_FEATURE_HAS_GBQUAL

#define IF_FEATURE_HAS_GBQUAL(Sft) \
if ((Sft).IsSetQual())

/// FEATURE_HAS_GBQUAL

#define FEATURE_HAS_GBQUAL(Sft) \
((Sft).IsSetQual())


/// FOR_EACH_GBQUAL_ON_SEQFEAT
/// EDIT_EACH_GBQUAL_ON_SEQFEAT
// CSeq_feat& as input, dereference with [const] CGb_qual& gbq = **itr;

#define GBQUAL_ON_SEQFEAT_Type      CSeq_feat::TQual
#define GBQUAL_ON_SEQFEAT_Test(Var) (Var).IsSetQual()
#define GBQUAL_ON_SEQFEAT_Get(Var)  (Var).GetQual()
#define GBQUAL_ON_SEQFEAT_Set(Var)  (Var).SetQual()

#define FOR_EACH_GBQUAL_ON_SEQFEAT(Itr, Var) \
FOR_EACH (GBQUAL_ON_SEQFEAT, Itr, Var)

#define FOR_EACH_GBQUAL_ON_FEATURE FOR_EACH_GBQUAL_ON_SEQFEAT

#define EDIT_EACH_GBQUAL_ON_SEQFEAT(Itr, Var) \
EDIT_EACH (GBQUAL_ON_SEQFEAT, Itr, Var)

#define EDIT_EACH_GBQUAL_ON_FEATURE EDIT_EACH_GBQUAL_ON_SEQFEAT


/// ERASE_GBQUAL_ON_FEATURE

#define ERASE_GBQUAL_ON_FEATURE(Itr, Sft) \
(Sft).SetQual().erase(Itr)


/// IF_FEATURE_HAS_SEQFEATXREF

#define IF_FEATURE_HAS_SEQFEATXREF(Sft) \
if ((Sft).IsSetXref())

/// FEATURE_HAS_SEQFEATXREF

#define FEATURE_HAS_SEQFEATXREF(Sft) \
((Sft).IsSetXref())


/// FOR_EACH_SEQFEATXREF_ON_SEQFEAT
/// EDIT_EACH_SEQFEATXREF_ON_SEQFEAT
// CSeq_feat& as input, dereference with [const] CSeqFeatXref& sfx = **itr;

#define SEQFEATXREF_ON_SEQFEAT_Type      CSeq_feat::TXref
#define SEQFEATXREF_ON_SEQFEAT_Test(Var) (Var).IsSetXref()
#define SEQFEATXREF_ON_SEQFEAT_Get(Var)  (Var).GetXref()
#define SEQFEATXREF_ON_SEQFEAT_Set(Var)  (Var).SetXref()

#define FOR_EACH_SEQFEATXREF_ON_SEQFEAT(Itr, Var) \
FOR_EACH (SEQFEATXREF_ON_SEQFEAT, Itr, Var)

#define FOR_EACH_SEQFEATXREF_ON_FEATURE FOR_EACH_SEQFEATXREF_ON_SEQFEAT

#define EDIT_EACH_SEQFEATXREF_ON_SEQFEAT(Itr, Var) \
EDIT_EACH (SEQFEATXREF_ON_SEQFEAT, Itr, Var)

#define EDIT_EACH_SEQFEATXREF_ON_FEATURE EDIT_EACH_SEQFEATXREF_ON_SEQFEAT


/// ERASE_SEQFEATXREF_ON_FEATURE

#define ERASE_SEQFEATXREF_ON_FEATURE(Itr, Sft) \
(Sft).SetXref().erase(Itr)


/// IF_FEATURE_HAS_DBXREF

#define IF_FEATURE_HAS_DBXREF(Sft) \
if ((Sft).IsSetDbxref())

/// FEATURE_HAS_DBXREF

#define FEATURE_HAS_DBXREF(Sft) \
((Sft).IsSetDbxref())


/// FOR_EACH_DBXREF_ON_SEQFEAT
/// EDIT_EACH_DBXREF_ON_SEQFEAT
// CSeq_feat& as input, dereference with [const] CDbtag& dbt = **itr;

#define DBXREF_ON_SEQFEAT_Type      CSeq_feat::TDbxref
#define DBXREF_ON_SEQFEAT_Test(Var) (Var).IsSetDbxref()
#define DBXREF_ON_SEQFEAT_Get(Var)  (Var).GetDbxref()
#define DBXREF_ON_SEQFEAT_Set(Var)  (Var).SetDbxref()

#define FOR_EACH_DBXREF_ON_SEQFEAT(Itr, Var) \
FOR_EACH (DBXREF_ON_SEQFEAT, Itr, Var)

#define FOR_EACH_DBXREF_ON_FEATURE FOR_EACH_DBXREF_ON_SEQFEAT

#define EDIT_EACH_DBXREF_ON_SEQFEAT(Itr, Var) \
EDIT_EACH (DBXREF_ON_SEQFEAT, Itr, Var)

#define EDIT_EACH_DBXREF_ON_FEATURE EDIT_EACH_DBXREF_ON_SEQFEAT


/// ERASE_DBXREF_ON_FEATURE

#define ERASE_DBXREF_ON_FEATURE(Itr, Sft) \
(Sft).SetDbxref().erase(Itr)


/// CSeqFeatData macros

/// IF_SEQFEATDATA_CHOICE_IS

#define IF_SEQFEATDATA_CHOICE_IS(Sfd, Chs) \
if ((Sfd).Which() == Chs)

/// SEQFEATDATA_CHOICE_IS

#define SEQFEATDATA_CHOICE_IS(Sfd, Chs) \
((Sfd).Which() == Chs)

/// SWITCH_ON_SEQFEATDATA_CHOICE

#define SEQFEATDATA_CHOICE_Test(Var) (Var).Which() != CSeqFeatData::e_not_set
#define SEQFEATDATA_CHOICE_Chs(Var)  (Var).Which()

#define SWITCH_ON_SEQFEATDATA_CHOICE(Var) \
SWITCH_ON (SEQFEATDATA_CHOICE, Var)


/// CSeqFeatXref macros

/// IF_SEQFEATXREF_CHOICE_IS

#define IF_SEQFEATXREF_CHOICE_IS(Sfx, Chs) \
if ((Sfx).IsSetData() && (Sfx).GetData().Which() == Chs)

/// SEQFEATXREF_CHOICE_IS

#define SEQFEATXREF_CHOICE_IS(Sfx, Chs) \
((Sfx).IsSetData() && (Sfx).GetData().Which() == Chs)

/// SWITCH_ON_SEQFEATXREF_CHOICE

#define SEQFEATXREF_CHOICE_Test(Var) (Var).IsSetData()
#define SEQFEATXREF_CHOICE_Chs(Var)  (Var).GetData().Which()

#define SWITCH_ON_SEQFEATXREF_CHOICE(Var) \
SWITCH_ON (SEQFEATXREF_CHOICE, Var)


/// CGene_ref macros

/// IF_GENE_HAS_SYNONYM

#define IF_GENE_HAS_SYNONYM(Grf) \
if ((Grf).IsSetSyn())

/// GENE_HAS_SYNONYM

#define GENE_HAS_SYNONYM(Grf) \
((Grf).IsSetSyn())


/// FOR_EACH_SYNONYM_ON_GENEREF
/// EDIT_EACH_SYNONYM_ON_GENEREF
// CGene_ref& as input, dereference with [const] string& str = *itr;

#define SYNONYM_ON_GENEREF_Type      CGene_ref::TSyn
#define SYNONYM_ON_GENEREF_Test(Var) (Var).IsSetSyn()
#define SYNONYM_ON_GENEREF_Get(Var)  (Var).GetSyn()
#define SYNONYM_ON_GENEREF_Set(Var)  (Var).SetSyn()

#define FOR_EACH_SYNONYM_ON_GENEREF(Itr, Var) \
FOR_EACH (SYNONYM_ON_GENEREF, Itr, Var)

#define FOR_EACH_SYNONYM_ON_GENE FOR_EACH_SYNONYM_ON_GENEREF

#define EDIT_EACH_SYNONYM_ON_GENEREF(Itr, Var) \
EDIT_EACH (SYNONYM_ON_GENEREF, Itr, Var)

#define EDIT_EACH_SYNONYM_ON_GENE EDIT_EACH_SYNONYM_ON_GENEREF


/// ERASE_SYNONYM_ON_GENE

#define ERASE_SYNONYM_ON_GENE(Itr, Grf) \
(Grf).SetSyn().erase(Itr)


/// IF_GENE_HAS_DBXREF

#define IF_GENE_HAS_DBXREF(Grf) \
if ((Grf).IsSetDb())

/// GENE_HAS_DBXREF

#define GENE_HAS_DBXREF(Grf) \
((Grf).IsSetDb())


/// FOR_EACH_DBXREF_ON_GENEREF
/// EDIT_EACH_DBXREF_ON_GENEREF
// CGene_ref& as input, dereference with [const] CDbtag& dbt = *itr;

#define DBXREF_ON_GENEREF_Type      CGene_ref::TDb
#define DBXREF_ON_GENEREF_Test(Var) (Var).IsSetDb()
#define DBXREF_ON_GENEREF_Get(Var)  (Var).GetDb()
#define DBXREF_ON_GENEREF_Set(Var)  (Var).SetDb()

#define FOR_EACH_DBXREF_ON_GENEREF(Itr, Var) \
FOR_EACH (DBXREF_ON_GENEREF, Itr, Var)

#define FOR_EACH_DBXREF_ON_GENE FOR_EACH_DBXREF_ON_GENEREF

#define EDIT_EACH_DBXREF_ON_GENEREF(Itr, Var) \
EDIT_EACH (DBXREF_ON_GENEREF, Itr, Var)

#define EDIT_EACH_DBXREF_ON_GENE EDIT_EACH_DBXREF_ON_GENEREF


/// ERASE_DBXREF_ON_GENE

#define ERASE_DBXREF_ON_GENE(Itr, Grf) \
(Grf).SetDb().erase(Itr)


/// CCdregion macros

/// IF_CDREGION_HAS_CODEBREAK

#define IF_CDREGION_HAS_CODEBREAK(Cdr) \
if ((Cdr).IsSetCode_break())

/// CDREGION_HAS_CODEBREAK

#define CDREGION_HAS_CODEBREAK(Cdr) \
((Cdr).IsSetCode_break())


/// FOR_EACH_CODEBREAK_ON_CDREGION
/// EDIT_EACH_CODEBREAK_ON_CDREGION
// CCdregion& as input, dereference with [const] CCode_break& cbk = **itr;

#define CODEBREAK_ON_CDREGION_Type      CCdregion::TCode_break
#define CODEBREAK_ON_CDREGION_Test(Var) (Var).IsSetCode_break()
#define CODEBREAK_ON_CDREGION_Get(Var)  (Var).GetCode_break()
#define CODEBREAK_ON_CDREGION_Set(Var)  (Var).SetCode_break()

#define FOR_EACH_CODEBREAK_ON_CDREGION(Itr, Var) \
FOR_EACH (CODEBREAK_ON_CDREGION, Itr, Var)

#define EDIT_EACH_CODEBREAK_ON_CDREGION(Itr, Var) \
EDIT_EACH (CODEBREAK_ON_CDREGION, Itr, Var)


/// ERASE_CODEBREAK_ON_CDREGION

#define ERASE_CODEBREAK_ON_CDREGION(Itr, Cdr) \
(Cdr).SetCode_break().erase(Itr)


/// CProt_ref macros

/// IF_PROT_HAS_NAME

#define IF_PROT_HAS_NAME(Prf) \
if ((Prf).IsSetName())

/// PROT_HAS_NAME

#define PROT_HAS_NAME(Prf) \
((Prf).IsSetName())


/// FOR_EACH_NAME_ON_PROTREF
/// EDIT_EACH_NAME_ON_PROTREF
// CProt_ref& as input, dereference with [const] string& str = *itr;

#define NAME_ON_PROTREF_Type      CProt_ref::TName
#define NAME_ON_PROTREF_Test(Var) (Var).IsSetName()
#define NAME_ON_PROTREF_Get(Var)  (Var).GetName()
#define NAME_ON_PROTREF_Set(Var)  (Var).SetName()

#define FOR_EACH_NAME_ON_PROTREF(Itr, Var) \
FOR_EACH (NAME_ON_PROTREF, Itr, Var)

#define FOR_EACH_NAME_ON_PROT FOR_EACH_NAME_ON_PROTREF

#define EDIT_EACH_NAME_ON_PROTREF(Itr, Var) \
EDIT_EACH (NAME_ON_PROTREF, Itr, Var)

#define EDIT_EACH_NAME_ON_PROT EDIT_EACH_NAME_ON_PROTREF


/// ERASE_NAME_ON_PROT

#define ERASE_NAME_ON_PROT(Itr, Prf) \
(Prf).SetName().erase(Itr)


/// IF_PROT_HAS_ECNUMBER

#define IF_PROT_HAS_ECNUMBER(Prf) \
if ((Prf).IsSetEc())

/// PROT_HAS_ECNUMBER

#define PROT_HAS_ECNUMBER(Prf) \
((Prf).IsSetEc())


/// FOR_EACH_ECNUMBER_ON_PROTREF
/// EDIT_EACH_ECNUMBER_ON_PROTREF
// CProt_ref& as input, dereference with [const] string& str = *itr;

#define ECNUMBER_ON_PROTREF_Type      CProt_ref::TEc
#define ECNUMBER_ON_PROTREF_Test(Var) (Var).IsSetEc()
#define ECNUMBER_ON_PROTREF_Get(Var)  (Var).GetEc()
#define ECNUMBER_ON_PROTREF_Set(Var)  (Var).SetEc()

#define FOR_EACH_ECNUMBER_ON_PROTREF(Itr, Var) \
FOR_EACH (ECNUMBER_ON_PROTREF, Itr, Var)

#define FOR_EACH_ECNUMBER_ON_PROT FOR_EACH_ECNUMBER_ON_PROTREF

#define EDIT_EACH_ECNUMBER_ON_PROTREF(Itr, Var) \
EDIT_EACH (ECNUMBER_ON_PROTREF, Itr, Var)

#define EDIT_EACH_ECNUMBER_ON_PROT EDIT_EACH_ECNUMBER_ON_PROTREF


/// ERASE_ECNUMBER_ON_PROT

#define ERASE_ECNUMBER_ON_PROT(Itr, Prf) \
(Prf).SetEc().erase(Itr)


/// IF_PROT_HAS_ACTIVITY

#define IF_PROT_HAS_ACTIVITY(Prf) \
if ((Prf).IsSetActivity())

/// PROT_HAS_ACTIVITY

#define PROT_HAS_ACTIVITY(Prf) \
((Prf).IsSetActivity())


/// FOR_EACH_ACTIVITY_ON_PROTREF
/// EDIT_EACH_ACTIVITY_ON_PROTREF
// CProt_ref& as input, dereference with [const] string& str = *itr;

#define ACTIVITY_ON_PROTREF_Type      CProt_ref::TActivity
#define ACTIVITY_ON_PROTREF_Test(Var) (Var).IsSetActivity()
#define ACTIVITY_ON_PROTREF_Get(Var)  (Var).GetActivity()
#define ACTIVITY_ON_PROTREF_Set(Var)  (Var).SetActivity()

#define FOR_EACH_ACTIVITY_ON_PROTREF(Itr, Var) \
FOR_EACH (ACTIVITY_ON_PROTREF, Itr, Var)

#define FOR_EACH_ACTIVITY_ON_PROT FOR_EACH_ACTIVITY_ON_PROTREF

#define EDIT_EACH_ACTIVITY_ON_PROTREF(Itr, Var) \
EDIT_EACH (ACTIVITY_ON_PROTREF, Itr, Var)

#define EDIT_EACH_ACTIVITY_ON_PROT EDIT_EACH_ACTIVITY_ON_PROTREF


/// ERASE_ACTIVITY_ON_PROT

#define ERASE_ACTIVITY_ON_PROT(Itr, Prf) \
(Prf).SetActivity().erase(Itr)


/// IF_PROT_HAS_DBXREF

#define IF_PROT_HAS_DBXREF(Prf) \
if ((Prf).IsSetDb())

/// PROT_HAS_DBXREF

#define PROT_HAS_DBXREF(Prf) \
((Prf).IsSetDb())


/// FOR_EACH_DBXREF_ON_PROTREF
/// EDIT_EACH_DBXREF_ON_PROTREF
// CProt_ref& as input, dereference with [const] CDbtag& dbt = *itr;

#define DBXREF_ON_PROTREF_Type      CProt_ref::TDb
#define DBXREF_ON_PROTREF_Test(Var) (Var).IsSetDb()
#define DBXREF_ON_PROTREF_Get(Var)  (Var).GetDb()
#define DBXREF_ON_PROTREF_Set(Var)  (Var).SetDb()

#define FOR_EACH_DBXREF_ON_PROTREF(Itr, Var) \
FOR_EACH (DBXREF_ON_PROTREF, Itr, Var)

#define FOR_EACH_DBXREF_ON_PROT FOR_EACH_DBXREF_ON_PROTREF

#define EDIT_EACH_DBXREF_ON_PROTREF(Itr, Var) \
EDIT_EACH (DBXREF_ON_PROTREF, Itr, Var)

#define EDIT_EACH_DBXREF_ON_PROT EDIT_EACH_DBXREF_ON_PROTREF


/// ERASE_DBXREF_ON_PROT

#define ERASE_DBXREF_ON_PROT(Itr, Prf) \
(Prf).SetDb().erase(Itr)


/// list <string> macros

/// IF_LIST_HAS_STRING

#define IF_LIST_HAS_STRING(Lst) \
if (! (Lst).empty())

/// LIST_HAS_STRING

#define LIST_HAS_STRING(Lst) \
(! (Lst).empty())


/// FOR_EACH_STRING_IN_LIST
/// EDIT_EACH_STRING_IN_LIST
// list <string>& as input, dereference with [const] string& str = *itr;

#define STRING_IN_LIST_Type      list <string>
#define STRING_IN_LIST_Test(Var) (! (Var).empty())
#define STRING_IN_LIST_Get(Var)  (Var)
#define STRING_IN_LIST_Set(Var)  (Var)

#define FOR_EACH_STRING_IN_LIST(Itr, Var) \
FOR_EACH (STRING_IN_LIST, Itr, Var)

#define EDIT_EACH_STRING_IN_LIST(Itr, Var) \
EDIT_EACH (STRING_IN_LIST, Itr, Var)


/// ERASE_STRING_IN_LIST

#define ERASE_STRING_IN_LIST(Itr, Lst) \
(Lst).erase(Itr)


/// <string> macros

/// IF_STRING_HAS_CHAR

#define IF_STRING_HAS_CHAR(Str) \
if (! (Str).empty())

/// STRING_HAS_CHAR

#define STRING_HAS_CHAR(Str) \
(! (Str).empty())


/// FOR_EACH_CHAR_IN_STRING
/// EDIT_EACH_CHAR_IN_STRING
// string& as input, dereference with [const] char& ch = *itr;

#define CHAR_IN_STRING_Type      string
#define CHAR_IN_STRING_Test(Var) (! (Var).empty())
#define CHAR_IN_STRING_Get(Var)  (Var)
#define CHAR_IN_STRING_Set(Var)  (Var)

#define FOR_EACH_CHAR_IN_STRING(Itr, Var) \
FOR_EACH (CHAR_IN_STRING, Itr, Var)

#define EDIT_EACH_CHAR_IN_STRING(Itr, Var) \
EDIT_EACH (CHAR_IN_STRING, Itr, Var)


/// ERASE_CHAR_IN_STRING

#define ERASE_CHAR_IN_STRING(Itr, Str) \
(Str).erase(Itr)



/// @}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif /* __SEQUENCE_MACROS__HPP__ */

