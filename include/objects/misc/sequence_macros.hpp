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

#define NCBI_SEQENTRY(TYPE) CSeq_entry::e_##Type
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

#define NCBI_SUBSRC(Type) CSubSource::eSubtype_##Type
typedef CSubSource::TSubtype TSUBSRC_SUBTYPE;

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

/// NCBI_ER_ITERATE base macro tests to see if loop should be entered
// If okay, calls ERASE_ITERATE for linear STL iteration

#define NCBI_ER_ITERATE(Test, Type, Var, Cont) \
if (! (Test)) {} else ERASE_ITERATE(Type, Var, Cont)

/// NCBI_SWITCH base macro tests to see if switch should be performed
// If okay, calls switch statement

#define NCBI_SWITCH(Test, Chs) \
if (! (Test)) {} else switch(Chs)


// "FOR_EACH_XXX_ON_YYY" does a linear const traversal of STL containers
// "EDIT_EACH_XXX_ON_YYY" does a linear non-const traversal of STL containers
//   with the possibility that the object can be deleted

// "SWITCH_ON_XXX_CHOICE" switches on the item subtype

// "ERASE_XXX_ON_YYY" deletes a specified object within an iterator

// Miscellaneous macros for testing objects include
// "IF_XXX_IS_YYY" or "IF_XXX_IS_YYY"
// "XXX_IS_YYY" or "XXX_HAS_YYY"
// "IF_XXX_CHOICE_IS"
// "XXX_CHOICE_IS"


/// CSeq_submit macros

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
// Takes const CSeq_submit& as input and makes iterator to CSeq_entry&
// Dereference with CSeq_entry& se = **iter;

#define EDIT_EACH_SEQENTRY_ON_SEQSUBMIT(Iter, Ss) \
NCBI_ER_ITERATE( \
    (Ss).IsEntrys(), \
    CSeq_submit::TData::TEntrys, \
    Iter, \
    (Ss).SetData().SetEntrys())

/// ERASE_SEQENTRY_ON_SEQSUBMIT

#define ERASE_SEQENTRY_ON_SEQSUBMIT(Iter, Ss) \
(Ss).SetData().SetEntrys().erase(Iter)


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
// Takes const CSeq_entry& as input and makes iterator to CSeqdesc&
// Dereference with CSeqdesc& desc = **iter

#define EDIT_EACH_DESCRIPTOR_ON_SEQENTRY(Iter, Seq) \
NCBI_ER_ITERATE( \
    (Seq).IsSetDescr(), \
    CSeq_descr::Tdata, \
    Iter, \
    (Seq).SetDescr().Set())

/// ERASE_DESCRIPTOR_ON_SEQENTRY

#define ERASE_DESCRIPTOR_ON_SEQENTRY(Iter, Seq) \
(Seq).SetDescr().Set().erase(Iter)


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
// Takes const CSeq_entry& as input and makes iterator to CSeq_annot&
// Dereference with CSeq_annot& annot = **iter;

#define EDIT_EACH_ANNOT_ON_SEQENTRY(Iter, Seq) \
NCBI_ER_ITERATE( \
    (Seq).IsSetAnnot(), \
    CSeq_entry::TAnnot, \
    Iter, \
    (Seq).SetAnnot())

/// ERASE_ANNOT_ON_SEQENTRY

#define ERASE_ANNOT_ON_SEQENTRY(Iter, Seq) \
(Seq).SetAnnot().erase(Iter)


/// CBioseq macros

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
// Takes const CBioseq& as input and makes iterator to CSeqdesc&
// Dereference with CSeqdesc& desc = **iter

#define EDIT_EACH_DESCRIPTOR_ON_BIOSEQ(Iter, Bsq) \
NCBI_ER_ITERATE( \
    (Bsq).IsSetDescr(), \
    CBioseq::TDescr::Tdata, \
    Iter, \
    (Bsq).SetDescr().Set())

/// ERASE_DESCRIPTOR_ON_BIOSEQ

#define ERASE_DESCRIPTOR_ON_BIOSEQ(Iter, Bsq) \
(Bsq).SetDescr().Set().erase(Iter)


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
// Takes const CBioseq& as input and makes iterator to CSeq_annot&
// Dereference with CSeq_annot& annot = **iter;

#define EDIT_EACH_ANNOT_ON_BIOSEQ(Iter, Bsq) \
NCBI_ER_ITERATE( \
    (Bsq).IsSetAnnot(), \
    CBioseq::TAnnot, \
    Iter, \
    (Bsq).SetAnnot())

/// ERASE_ANNOT_ON_BIOSEQ

#define ERASE_ANNOT_ON_BIOSEQ(Iter, Bsq) \
(Bsq).SetAnnot().erase(Iter)


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
// Takes const CBioseq& as input and makes iterator to CSeq_id&
// Dereference with CSeq_id& sid = **iter;

#define EDIT_EACH_SEQID_ON_BIOSEQ(Iter, Bsq) \
NCBI_ER_ITERATE( \
    (Bsq).IsSetId(), \
    CBioseq::TId, \
    Iter, \
    (Bsq).SetId())

/// ERASE_SEQID_ON_BIOSEQ

#define ERASE_SEQID_ON_BIOSEQ(Iter, Bsq) \
(Bsq).SetId().erase(Iter)


/// CSeq_id macros

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


/// CBioseq_set macros

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
// Takes const CBioseq_set& as input and makes iterator to CSeqdesc&
// Dereference with CSeqdesc& desc = **iter;

#define EDIT_EACH_DESCRIPTOR_ON_SEQSET(Iter, Bss) \
NCBI_ER_ITERATE( \
    (Bss).IsSetDescr(), \
    CBioseq_set::TDescr::Tdata, \
    Iter, \
    (Bss).SetDescr().Set())

/// ERASE_DESCRIPTOR_ON_SEQSET

#define ERASE_DESCRIPTOR_ON_SEQSET(Iter, Bss) \
(Bss).SetDescr().Set().erase(Iter)


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
// Takes const CBioseq_set& as input and makes iterator to CSeq_annot&
// Dereference with CSeq_annot& annot = **iter;

#define EDIT_EACH_ANNOT_ON_SEQSET(Iter, Bss) \
NCBI_ER_ITERATE( \
    (Bss).IsSetAnnot(), \
    CBioseq_set::TAnnot, \
    Iter, \
    (Bss).SetAnnot())

/// ERASE_ANNOT_ON_SEQSET

#define ERASE_ANNOT_ON_SEQSET(Iter, Bss) \
(Bss).SetAnnot().erase(Iter)


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
// Takes const CBioseq_set& as input and makes iterator to CSeq_entry&
// Dereference with CSeq_entry& se = **iter;

#define EDIT_EACH_SEQENTRY_ON_SEQSET(Iter, Bss) \
NCBI_ER_ITERATE( \
    (Bss).IsSetSeq_set(), \
    CBioseq_set::TSeq_set, \
    Iter, \
    (Bss).SetSeq_set())

/// ERASE_SEQENTRY_ON_SEQSET

#define ERASE_SEQENTRY_ON_SEQSET(Iter, Bss) \
(Bss).SetSeq_set().erase(Iter)


/// CSeq_annot macros

/// IF_ANNOT_IS_FEATURE

#define IF_ANNOT_IS_FEATURE(San) \
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
// Takes const CSeq_annot& as input and makes iterator to CSeq_feat&
// Dereference with CSeq_feat& feat = **iter;

#define EDIT_EACH_FEATURE_ON_ANNOT(Iter, San) \
NCBI_ER_ITERATE( \
    (San).IsFtable(), \
    CSeq_annot::TData::TFtable, \
    Iter, \
    (San).SetData().SetFtable())

/// ERASE_FEATURE_ON_ANNOT

#define ERASE_FEATURE_ON_ANNOT(Iter, San) \
(San).SetData().SetFtable().erase(Iter)


/// IF_ANNOT_IS_ALIGN

#define IF_ANNOT_IS_ALIGN(San) \
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
// Takes const CSeq_annot& as input and makes iterator to CSeq_align&
// Dereference with CSeq_align& align = **iter;

#define EDIT_EACH_ALIGN_ON_ANNOT(Iter, San) \
NCBI_ER_ITERATE( \
    (San).IsAlign(), \
    CSeq_annot::TData::TAlign, \
    Iter, \
    (San).SetData().SetAlign())

/// ERASE_ALIGN_ON_ANNOT

#define ERASE_ALIGN_ON_ANNOT(Iter, San) \
(San).SetData().SetAlign().erase(Iter)


/// IF_ANNOT_IS_GRAPH

#define IF_ANNOT_IS_GRAPH(San) \
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
// Takes const CSeq_annot& as input and makes iterator to CSeq_graph&
// Dereference with CSeq_graph& graph = **iter;

#define EDIT_EACH_GRAPH_ON_ANNOT(Iter, San) \
NCBI_ER_ITERATE( \
    (San).IsGraph(), \
    CSeq_annot::TData::TGraph, \
    Iter, \
    (San).SetData().SetGraph())

/// ERASE_GRAPH_ON_ANNOT

#define ERASE_GRAPH_ON_ANNOT(Iter, San) \
(San).SetData().SetGraph().erase(Iter)


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
// Takes const CSeq_annot& as input and makes iterator to CAnnotdesc&
// Dereference with CAnnotdesc& desc = **iter;

#define EDIT_EACH_ANNOTDESC_ON_ANNOT(Iter, San) \
NCBI_ER_ITERATE( \
    (San).IsSetDesc(), \
    CSeq_annot::TDesc, \
    Iter, \
    (San).SetDesc())

/// ERASE_ANNOTDESC_ON_ANNOT

#define ERASE_ANNOTDESC_ON_ANNOT(Iter, San) \
(San).SetDesc().erase(Iter)


/// CAnnotdesc macros

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


/// CSeq_descr macros

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
// Takes const CSeq_descr& as input and makes iterator to CSeqdesc&
// Dereference with CSeqdesc& desc = **iter;

#define EDIT_EACH_DESCRIPTOR_ON_DESCR(Iter, Descr) \
NCBI_ER_ITERATE( \
    (Descr).IsSet(), \
    CSeq_descr::Tdata, \
    Iter, \
    (Descr).Set())

/// ERASE_DESCRIPTOR_ON_DESCR

#define ERASE_DESCRIPTOR_ON_DESCR(Iter, Descr) \
(Descr).Set().erase(Iter)


/// CSeqdesc macros

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


/// CMolInfo macros

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


/// CBioSource macros

/// IF_BIOSOURCE_GENOME_IS

#define IF_BIOSOURCE_GENOME_IS(Bsc, Chs) \
if ((Bsc).IsSetGenome() && (Bsc).GetGenome() == Chs)

/// BIOSOURCE_GENOME_IS

#define BIOSOURCE_GENOME_IS(Bsc, Chs) \
((Bsc).IsSetGenome() && (Bsc).GetGenome() == Chs)

/// SWITCH_ON_BIOSOURCE_GENOME

#define SWITCH_ON_BIOSOURCE_GENOME(Bsc) \
NCBI_SWITCH( \
    (Bsc).IsSetGenome(), \
    (Bsc).GetGenome())


/// IF_BIOSOURCE_ORIGIN_IS

#define IF_BIOSOURCE_ORIGIN_IS(Bsc, Chs) \
if ((Bsc).IsSetOrigin() && (Bsc).GetOrigin() == Chs)

/// BIOSOURCE_ORIGIN_IS

#define BIOSOURCE_ORIGIN_IS(Bsc, Chs) \
((Bsc).IsSetOrigin() && (Bsc).GetOrigin() == Chs)

/// SWITCH_ON_BIOSOURCE_ORIGIN

#define SWITCH_ON_BIOSOURCE_ORIGIN(Bsc) \
NCBI_SWITCH( \
    (Bsc).IsSetOrigin(), \
    (Bsc).GetOrigin())


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
// Takes const CBioSource& as input and makes iterator to const CSubSource&
// Dereference with const CSubSource& sbs = **iter

#define FOR_EACH_SUBSOURCE_ON_BIOSOURCE(Iter, Bsc) \
NCBI_CS_ITERATE( \
    (Bsc).IsSetSubtype(), \
    CBioSource::TSubtype, \
    Iter, \
    (Bsc).GetSubtype())

/// EDIT_EACH_SUBSOURCE_ON_BIOSOURCE
// Takes const CBioSource& as input and makes iterator to CSubSource&
// Dereference with CSubSource& sbs = **iter

#define EDIT_EACH_SUBSOURCE_ON_BIOSOURCE(Iter, Bsc) \
NCBI_ER_ITERATE( \
    (Bsc).IsSetSubtype(), \
    CBioSource::TSubtype, \
    Iter, \
    (Bsc).SetSubtype())

/// ERASE_SUBSOURCE_ON_BIOSOURCE

#define ERASE_SUBSOURCE_ON_BIOSOURCE(Iter, Bsc) \
(Bsc).SetSubtype().erase(Iter)


/// IF_BIOSOURCE_HAS_ORGMOD

#define IF_BIOSOURCE_HAS_ORGMOD(Bsc) \
if ((Bsc).IsSetOrgMod())

/// BIOSOURCE_HAS_ORGMOD

#define BIOSOURCE_HAS_ORGMOD(Bsc) \
((Bsc).IsSetOrgMod())

/// FOR_EACH_ORGMOD_ON_BIOSOURCE
// Takes const CBioSource& as input and makes iterator to const COrgMod&
// Dereference with const COrgMod& omd = **iter

#define FOR_EACH_ORGMOD_ON_BIOSOURCE(Iter, Bsc) \
NCBI_CS_ITERATE( \
    (Bsc).IsSetOrgMod(), \
    COrgName::TMod, \
    Iter, \
    (Bsc).GetOrgname().GetMod())

/// EDIT_EACH_ORGMOD_ON_BIOSOURCE
// Takes const CBioSource& as input and makes iterator to COrgMod&
// Dereference with COrgMod& omd = **iter

#define EDIT_EACH_ORGMOD_ON_BIOSOURCE(Iter, Bsc) \
NCBI_ER_ITERATE( \
    (Bsc).IsSetOrgMod(), \
    COrgName::TMod, \
    Iter, \
    (Bsc).SetOrgname().SetMod())

/// ERASE_ORGMOD_ON_BIOSOURCE

#define ERASE_ORGMOD_ON_BIOSOURCE(Iter, Bsc) \
(Bsc).SetOrgname().SetMod().erase(Iter)


/// COrg_ref macros

/// IF_ORGREF_HAS_ORGMOD

#define IF_ORGREF_HAS_ORGMOD(Org) \
if ((Org).IsSetOrgMod())

/// ORGREF_HAS_ORGMOD

#define ORGREF_HAS_ORGMOD(Org) \
((Org).IsSetOrgMod())

/// FOR_EACH_ORGMOD_ON_ORGREF
// Takes const COrg_ref& as input and makes iterator to const COrgMod&
// Dereference with const COrgMod& omd = **iter

#define FOR_EACH_ORGMOD_ON_ORGREF(Iter, Org) \
NCBI_CS_ITERATE( \
    (Org).IsSetOrgMod(), \
    COrgName::TMod, \
    Iter, \
    (Org).GetOrgname().GetMod())

/// EDIT_EACH_ORGMOD_ON_ORGREF
// Takes const COrg_ref& as input and makes iterator to COrgMod&
// Dereference with COrgMod& omd = **iter

#define EDIT_EACH_ORGMOD_ON_ORGREF(Iter, Org) \
NCBI_ER_ITERATE( \
    (Org).IsSetOrgMod(), \
    COrgName::TMod, \
    Iter, \
    (Org).SetOrgname().SetMod())

/// ERASE_ORGMOD_ON_ORGREF

#define ERASE_ORGMOD_ON_ORGREF(Iter, Org) \
(Org).SetOrgname().SetMod().erase(Iter)


/// IF_ORGREF_HAS_DBXREF

#define IF_ORGREF_HAS_DBXREF(Org) \
if ((Org).IsSetDb())

/// ORGREF_HAS_DBXREF

#define ORGREF_HAS_DBXREF(Org) \
((Org).IsSetDb())

/// FOR_EACH_DBXREF_ON_ORGREF
// Takes const COrg_ref& as input and makes iterator to const CDbtag&
// Dereference with const CDbtag& dbt = **iter

#define FOR_EACH_DBXREF_ON_ORGREF(Iter, Org) \
NCBI_CS_ITERATE( \
    (Org).IsSetDb(), \
    COrg_ref::TDb, \
    Iter, \
    (Org).GetDb())

/// EDIT_EACH_DBXREF_ON_ORGREF
// Takes const COrg_ref& as input and makes iterator to CDbtag&
// Dereference with CDbtag& dbt = **iter

#define EDIT_EACH_DBXREF_ON_ORGREF(Iter, Org) \
NCBI_ER_ITERATE( \
    (Org).IsSetDb(), \
    COrg_ref::TDb, \
    Iter, \
    (Org).SetDb())

/// ERASE_DBXREF_ON_ORGREF

#define ERASE_DBXREF_ON_ORGREF(Iter, Org) \
(Org).SetDb().erase(Iter)


/// IF_ORGREF_HAS_MOD

#define IF_ORGREF_HAS_MOD(Org) \
if ((Org).IsSetMod())

/// ORGREF_HAS_MOD

#define ORGREF_HAS_MOD(Org) \
((Org).IsSetMod())

/// FOR_EACH_MOD_ON_ORGREF
// Takes const COrg_ref& as input and makes iterator to const string&
// Dereference with const string& str = *iter

#define FOR_EACH_MOD_ON_ORGREF(Iter, Org) \
NCBI_CS_ITERATE( \
    (Org).IsSetMod(), \
    COrg_ref::TMod, \
    Iter, \
    (Org).GetMod())

/// EDIT_EACH_MOD_ON_ORGREF
// Takes const COrg_ref& as input and makes iterator to string&
// Dereference with string& str = *iter

#define EDIT_EACH_MOD_ON_ORGREF(Iter, Org) \
NCBI_ER_ITERATE( \
    (Org).IsSetMod(), \
    COrg_ref::TMod, \
    Iter, \
    (Org).SetMod())

/// ERASE_MOD_ON_ORGREF

#define ERASE_MOD_ON_ORGREF(Iter, Org) \
(Org).SetMod().erase(Iter)


/// COrgName macros

/// IF_ORGNAME_CHOICE_IS

#define IF_ORGNAME_CHOICE_IS(Onm, Chs) \
if ((Onm).IsSetName() && (Onm).GetName().Which() == Chs)

/// ORGNAME_CHOICE_IS

#define ORGNAME_CHOICE_IS(Onm, Chs) \
((Onm).IsSetName() && (Onm).GetName().Which() == Chs)

/// SWITCH_ON_MOLINFO_COMPLETENESS

#define SWITCH_ON_ORGNAME_CHOICE(Onm) \
NCBI_SWITCH( \
    (Onm).IsSetName() && (Onm).GetName().Which() != COrgName::e_not_set, \
    (Onm).GetName().Which())


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
// Takes const COrgName& as input and makes iterator to COrgMod&
// Dereference with COrgMod& omd = **iter

#define EDIT_EACH_ORGMOD_ON_ORGNAME(Iter, Onm) \
NCBI_ER_ITERATE( \
    (Onm).IsSetMod(), \
    COrgName::TMod, \
    Iter, \
    (Onm).SetMod())

/// ERASE_ORGMOD_ON_ORGNAME

#define ERASE_ORGMOD_ON_ORGNAME(Iter, Onm) \
(Onm).SetMod().erase(Iter)


/// CSubSource macros

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


/// COrgMod macros

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


/// CPubdesc macros

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
// Takes const CPubdesc& as input and makes iterator to CPub&
// Dereference with CPub& pub = **iter;

#define EDIT_EACH_PUB_ON_PUBDESC(Iter, Pbd) \
NCBI_ER_ITERATE( \
    (Pbd).IsSetPub() && \
        (Pbd).GetPub().IsSet(), \
    CPub_equiv::Tdata, \
    Iter, \
    (Pbd).SetPub().Set())

/// ERASE_PUB_ON_PUBDESC

#define ERASE_PUB_ON_PUBDESC(Iter, Pbd) \
(Pbd).SetPub().Set().erase(Iter)


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

/// EDIT_EACH_AUTHOR_ON_PUB
// Takes const CPub& as input and makes iterator to CAuthor&
// Dereference with CAuthor& auth = **iter;

#define EDIT_EACH_AUTHOR_ON_PUB(Iter, Pub) \
NCBI_ER_ITERATE( \
    (Pub).IsSetAuthors() && \
        (Pub).GetAuthors().IsSetNames() && \
        (Pub).GetAuthors().GetNames().IsStd() , \
    CAuth_list::C_Names::TStd, \
    Iter, \
    (Pub).SetAuthors().SetNames().SetStd())

/// ERASE_AUTHOR_ON_PUB

#define ERASE_AUTHOR_ON_PUB(Iter, Pub) \
(Pub).SetAuthors().SetNames().SetStd().erase(Iter)


/// CGB_block macros

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
// Takes const CGB_block& as input and makes iterator to string&
// Dereference with string& str = *iter;

#define EDIT_EACH_EXTRAACCN_ON_GENBANKBLOCK(Iter, Gbk) \
NCBI_ER_ITERATE( \
    (Gbk).IsSetExtra_accessions(), \
    CGB_block::TExtra_accessions, \
    Iter, \
    (Gbk).SetExtra_accessions())

/// ERASE_EXTRAACCN_ON_GENBANKBLOCK

#define ERASE_EXTRAACCN_ON_GENBANKBLOCK(Iter, Gbk) \
(Gbk).SetExtra_accessions().erase(Iter)


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
// Takes const CGB_block& as input and makes iterator to string&
// Dereference with string& str = *iter;

#define EDIT_EACH_KEYWORD_ON_GENBANKBLOCK(Iter, Gbk) \
NCBI_ER_ITERATE( \
    (Gbk).IsSetKeywords(), \
    CGB_block::TKeywords, \
    Iter, \
    (Gbk).SetKeywords())

/// ERASE_KEYWORD_ON_GENBANKBLOCK

#define ERASE_KEYWORD_ON_GENBANKBLOCK(Iter, Gbk) \
(Gbk).SetKeywords().erase(Iter)


/// CEMBL_block macros

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
// Takes const CEMBL_block& as input and makes iterator to string&
// Dereference with string& str = *iter;

#define EDIT_EACH_EXTRAACCN_ON_EMBLBLOCK(Iter, Ebk) \
NCBI_ER_ITERATE( \
    (Ebk).IsSetExtra_acc(), \
    CEMBL_block::TExtra_acc, \
    Iter, \
    (Ebk).SetExtra_acc())

/// ERASE_EXTRAACCN_ON_EMBLBLOCK

#define ERASE_EXTRAACCN_ON_EMBLBLOCK(Iter, Ebk) \
(Ebk).SetExtra_acc().erase(Iter)


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
// Takes const CEMBL_block& as input and makes iterator to string&
// Dereference with string& str = *iter;

#define EDIT_EACH_KEYWORD_ON_EMBLBLOCK(Iter, Ebk) \
NCBI_ER_ITERATE( \
    (Ebk).IsSetKeywords(), \
    CEMBL_block::TKeywords, \
    Iter, \
    (Ebk).SetKeywords())

/// ERASE_KEYWORD_ON_EMBLBLOCK

#define ERASE_KEYWORD_ON_EMBLBLOCK(Iter, Ebk) \
(Ebk).SetKeywords().erase(Iter)


/// CPDB_block macros

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
// Takes const CPDB_block& as input and makes iterator to string&
// Dereference with string& str = *iter;

#define EDIT_EACH_COMPOUND_ON_PDBBLOCK(Iter, Pbk) \
NCBI_ER_ITERATE( \
    (Pbk).IsSetCompound(), \
    CPDB_block::TCompound, \
    Iter, \
    (Pbk).SetCompound())

/// ERASE_COMPOUND_ON_PDBBLOCK

#define ERASE_COMPOUND_ON_PDBBLOCK(Iter, Pbk) \
(Pbk).SetCompound().erase(Iter)


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
// Takes const CPDB_block& as input and makes iterator to string&
// Dereference with string& str = *iter;

#define EDIT_EACH_SOURCE_ON_PDBBLOCK(Iter, Pbk) \
NCBI_ER_ITERATE( \
    (Pbk).IsSetSource(), \
    CPDB_block::TSource, \
    Iter, \
    (Pbk).SetSource())

/// ERASE_SOURCE_ON_PDBBLOCK

#define ERASE_SOURCE_ON_PDBBLOCK(Iter, Pbk) \
(Pbk).SetSource().erase(Iter)


/// CSeq_feat macros

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
// Takes const CSeq_feat& as input and makes iterator to CGb_qual&
// Dereference with CGb_qual& gbq = **iter;

#define EDIT_EACH_GBQUAL_ON_FEATURE(Iter, Sft) \
NCBI_ER_ITERATE( \
    (Sft).IsSetQual(), \
    CSeq_feat::TQual, \
    Iter, \
    (Sft).SetQual())

/// ERASE_GBQUAL_ON_FEATURE

#define ERASE_GBQUAL_ON_FEATURE(Iter, Sft) \
(Sft).SetQual().erase(Iter)


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
// Takes const CSeq_feat& as input and makes iterator to CSeqFeatXref&
// Dereference with CSeqFeatXref& sfx = **iter;

#define EDIT_EACH_SEQFEATXREF_ON_FEATURE(Iter, Sft) \
NCBI_ER_ITERATE( \
    (Sft).IsSetXref(), \
    CSeq_feat::TXref, \
    Iter, \
    (Sft).SetXref())

/// ERASE_SEQFEATXREF_ON_FEATURE

#define ERASE_SEQFEATXREF_ON_FEATURE(Iter, Sft) \
(Sft).SetXref().erase(Iter)


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
// Takes const CSeq_feat& as input and makes iterator to CDbtag&
// Dereference with CDbtag& dbt = **iter;

#define EDIT_EACH_DBXREF_ON_FEATURE(Iter, Sft) \
NCBI_ER_ITERATE( \
    (Sft).IsSetDbxref(), \
    CSeq_feat::TDbxref, \
    Iter, \
    (Sft).SetDbxref())

/// ERASE_DBXREF_ON_FEATURE

#define ERASE_DBXREF_ON_FEATURE(Iter, Sft) \
(Sft).SetDbxref().erase(Iter)


/// CSeqFeatData macros

/// IF_SEQFEATDATA_CHOICE_IS

#define IF_SEQFEATDATA_CHOICE_IS(Sfd, Chs) \
if ((Sfd).Which() == Chs)

/// SEQFEATDATA_CHOICE_IS

#define SEQFEATDATA_CHOICE_IS(Sfd, Chs) \
((Sfd).Which() == Chs)

/// SWITCH_ON_SEQFEATDATA_CHOICE

#define SWITCH_ON_SEQFEATDATA_CHOICE(Sfd) \
NCBI_SWITCH( \
    (Sfd).Which() != CSeqFeatData::e_not_set, \
    (Sfd).Which())


/// CSeqFeatXref macros

/// IF_SEQFEATXREF_CHOICE_IS

#define IF_SEQFEATXREF_CHOICE_IS(Sfx, Chs) \
if ((Sfx).IsSetData() && (Sfx).GetData().Which() == Chs)

/// SEQFEATXREF_CHOICE_IS

#define SEQFEATXREF_CHOICE_IS(Sfx, Chs) \
((Sfx).IsSetData() && (Sfx).GetData().Which() == Chs)

/// SWITCH_ON_SEQFEATXREF_CHOICE

#define SWITCH_ON_SEQFEATXREF_CHOICE(Sfx) \
NCBI_SWITCH( \
    (Sfx).IsSetData(), \
    (Sfx).GetData().Which())


/// CGene_ref macros

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
// Takes const CGene_ref& as input and makes iterator to string&
// Dereference with string& str = *iter;

#define EDIT_EACH_SYNONYM_ON_GENE(Iter, Grf) \
NCBI_ER_ITERATE( \
    (Grf).IsSetSyn(), \
    CGene_ref::TSyn, \
    Iter, \
    (Grf).SetSyn())

/// ERASE_SYNONYM_ON_GENE

#define ERASE_SYNONYM_ON_GENE(Iter, Grf) \
(Grf).SetSyn().erase(Iter)


/// IF_GENE_HAS_DBXREF

#define IF_GENE_HAS_DBXREF(Grf) \
if ((Grf).IsSetDb())

/// GENE_HAS_DBXREF

#define GENE_HAS_DBXREF(Grf) \
((Grf).IsSetDb())

/// FOR_EACH_DBXREF_ON_GENE
// Takes const CGene_ref& as input and makes iterator to const CDbtag&
// Dereference with const CDbtag& dbt = *iter;

#define FOR_EACH_DBXREF_ON_GENE(Iter, Grf) \
NCBI_CS_ITERATE( \
    (Grf).IsSetDb(), \
    CGene_ref::TDb, \
    Iter, \
    (Grf).GetDb())

/// EDIT_EACH_DBXREF_ON_GENE
// Takes const CGene_ref& as input and makes iterator to CDbtag&
// Dereference with CDbtag& dbt = *iter;

#define EDIT_EACH_DBXREF_ON_GENE(Iter, Grf) \
NCBI_CS_ITERATE( \
    (Grf).IsSetDb(), \
    CGene_ref::TDb, \
    Iter, \
    (Grf).SetDb())

/// ERASE_DBXREF_ON_GENE

#define ERASE_DBXREF_ON_GENE(Iter, Grf) \
(Grf).SetDb().erase(Iter)


/// CCdregion macros

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
// Takes const CCdregion& as input and makes iterator to CCode_break&
// Dereference with CCode_break& cbk = **iter;

#define EDIT_EACH_CODEBREAK_ON_CDREGION(Iter, Cdr) \
NCBI_ER_ITERATE( \
    (Cdr).IsSetCode_break(), \
    CCdregion::TCode_break, \
    Iter, \
    (Cdr).SetCode_break())

/// ERASE_CODEBREAK_ON_CDREGION

#define ERASE_CODEBREAK_ON_CDREGION(Iter, Cdr) \
(Cdr).SetCode_break().erase(Iter)


/// CProt_ref macros

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
// Takes const CProt_ref& as input and makes iterator to string&
// Dereference with string& str = *iter;

#define EDIT_EACH_NAME_ON_PROT(Iter, Prf) \
NCBI_ER_ITERATE( \
    (Prf).IsSetName(), \
    CProt_ref::TName, \
    Iter, \
    (Prf).SetName())

/// ERASE_NAME_ON_PROT

#define ERASE_NAME_ON_PROT(Iter, Prf) \
(Prf).SetName().erase(Iter)


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
// Takes const CProt_ref& as input and makes iterator to string&
// Dereference with string& str = *iter;

#define EDIT_EACH_ECNUMBER_ON_PROT(Iter, Prf) \
NCBI_ER_ITERATE( \
    (Prf).IsSetEc(), \
    CProt_ref::TEc, \
    Iter, \
    (Prf).SetEc())

/// ERASE_ECNUMBER_ON_PROT

#define ERASE_ECNUMBER_ON_PROT(Iter, Prf) \
(Prf).SetEc().erase(Iter)


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
// Takes const CProt_ref& as input and makes iterator to string&
// Dereference with string& str = *iter;

#define EDIT_EACH_ACTIVITY_ON_PROT(Iter, Prf) \
NCBI_ER_ITERATE( \
    (Prf).IsSetActivity(), \
    CProt_ref::TActivity, \
    Iter, \
    (Prf).SetActivity())

/// ERASE_ACTIVITY_ON_PROT

#define ERASE_ACTIVITY_ON_PROT(Iter, Prf) \
(Prf).SetActivity().erase(Iter)


/// IF_PROT_HAS_DBXREF

#define IF_PROT_HAS_DBXREF(Prf) \
if ((Prf).IsSetDb())

/// PROT_HAS_DBXREF

#define PROT_HAS_DBXREF(Prf) \
((Prf).IsSetDb())

/// FOR_EACH_DBXREF_ON_PROT
// Takes const CProt_ref& as input and makes iterator to const CDbtag&
// Dereference with const CDbtag& dbt = *iter;

#define FOR_EACH_DBXREF_ON_PROT(Iter, Prf) \
NCBI_CS_ITERATE( \
    (Prf).IsSetDb(), \
    CProt_ref::TDb, \
    Iter, \
    (Prf).GetDb())

/// EDIT_EACH_DBXREF_ON_PROT
// Takes const CProt_ref& as input and makes iterator to CDbtag&
// Dereference with CDbtag& dbt = *iter;

#define EDIT_EACH_DBXREF_ON_PROT(Iter, Prf) \
NCBI_ER_ITERATE( \
    (Prf).IsSetDb(), \
    CProt_ref::TDb, \
    Iter, \
    (Prf).SetDb())

/// ERASE_DBXREF_ON_PROT

#define ERASE_DBXREF_ON_PROT(Iter, Prf) \
(Prf).SetDb().erase(Iter)


/// list <string> macros

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
// Takes const list <string>& as input and makes iterator to string&
// Dereference with string& str = *iter;

#define EDIT_EACH_STRING_IN_LIST(Iter, Lst) \
NCBI_ER_ITERATE( \
    (! (Lst).empty()), \
    list <string>, \
    Iter, \
    (Lst))

/// ERASE_STRING_IN_LIST

#define ERASE_STRING_IN_LIST(Iter, Lst) \
(Lst).erase(Iter)


/// <string> macros

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
// Takes const string& as input and makes iterator to char&
// Dereference with char& ch = *iter;

#define EDIT_EACH_CHAR_IN_STRING(Iter, Str) \
NCBI_CS_ITERATE( \
    (! (Str).empty()), \
    string, \
    Iter, \
    (Str))

/// ERASE_CHAR_IN_STRING

#define ERASE_CHAR_IN_STRING(Iter, Str) \
(Str).erase(Iter)



/// @}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif /* __SEQUENCE_MACROS__HPP__ */

