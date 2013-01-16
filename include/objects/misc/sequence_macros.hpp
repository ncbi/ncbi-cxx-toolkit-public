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
#include <serial/iterator.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/// @NAME Convenience macros for NCBI objects
/// @{



/////////////////////////////////////////////////////////////////////////////
/// Macros and typedefs for object subtypes
/////////////////////////////////////////////////////////////////////////////


/// CSeq_submit definitions

#define NCBI_SEQSUBMIT(Type) CSeq_submit::TData::e_##Type
typedef CSeq_submit::TData::E_Choice TSEQSUBMIT_CHOICE;

//   Entrys     Annots


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


/// CUser_field definitions

#define NCBI_USERFIELD(Type) CUser_field::TData::e_##Type
typedef CUser_field::C_Data::E_Choice TUSERFIELD_CHOICE;

//   Str        Int         Real     Bool      Os
//   Object     Strs        Ints     Reals     Oss
//   Fields     Objects


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
//   Clone


/// CProt_ref definitions

#define NCBI_PROTREF(Type) CProt_ref::eProcessed_##Type
typedef CProt_ref::EProcessed TPROTREF_PROCESSED;

//   preprotein     mature     signal_peptide     transit_peptide


/// CRNA_ref definitions

#define NCBI_RNAREF(Type) CRNA_ref::eType_##Type
typedef CRNA_ref::EType TRNAREF_TYPE;

//   premsg     mRNA      tRNA      rRNA        snRNA     scRNA
//   snoRNA     ncRNA     tmRNA     miscRNA     other

#define NCBI_RNAEXT(Type) CRNA_ref::C_Ext::e_##Type
typedef CRNA_ref::C_Ext::E_Choice TRNAREF_EXT;

//   Name     TRNA      Gen

#define NCBI_PERSONID(Type) CPerson_id::e_##Type
typedef CPerson_id::E_Choice TPERSONID_TYPE;

//  Dendiag   Denseg    Std     Packed
//  Disc      Spliced   Sparse

#define NCBI_SEGTYPE(Type) CSeq_align::C_Segs::e_##Type
typedef CSeq_align::C_Segs::E_Choice TSEGTYPE_TYPE;

// not_set    one       two     three

#define NCBI_CDSFRAME(Type) CCdregion::eFrame_##Type
typedef CCdregion::EFrame TCDSFRAME_TYPE;

//  not_set  Null        Empty
//  Whole    Int         Packed_int
//  Pnt      Packed_pnt  Mix
//  Equiv    Bond        Feat

#define NCBI_SEQLOC(Type) CSeq_loc::e_##Type
typedef CSeq_loc::E_Choice TSEQLOC_TYPE;

// not_set
// nuc_prot          segset            conset
// parts             gibb              gi
// genbank           pir               pub_set
// equiv             swissprot         pdb_entry
// mut_set           pop_set           phy_set
// eco_set           gen_prod_set      wgs_set
// named_annot       named_annot_prod  read_set
// paired_end_reads  small_genome_set  other

#define NCBI_BIOSEQSETCLASS(Type) CBioseq_set::eClass_##Type
typedef CBioseq_set::EClass TBIOSEQSETCLASS_TYPE;

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



/////////////////////////////////////////////////////////////////////////////
/// Macros to recursively explore within NCBI data model objects
/////////////////////////////////////////////////////////////////////////////


/// NCBI_SERIAL_TEST_EXPLORE base macro tests to see if loop should be entered
// If okay, calls CTypeConstIterator for recursive exploration

#define NCBI_SERIAL_TEST_EXPLORE(Test, Type, Var, Cont) \
if (! (Test)) {} else for (CTypeConstIterator<Type> Var (Cont); Var; ++Var)


/// VISIT_WITHIN_SEQENTRY base macro makes recursive iterator with generated components
/// VISIT_WITHIN_SEQSET base macro makes recursive iterator with generated components

#define VISIT_WITHIN_SEQENTRY(Typ, Itr, Var) \
NCBI_SERIAL_TEST_EXPLORE ((Var).Which() != CSeq_entry::e_not_set, Typ, Itr, (Var))

#define VISIT_WITHIN_SEQSET(Typ, Itr, Var) \
NCBI_SERIAL_TEST_EXPLORE ((Var).IsSetSeq_set(), Typ, Itr, (Var))


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



/////////////////////////////////////////////////////////////////////////////
/// Macros to iterate over standard template containers (non-recursive)
/////////////////////////////////////////////////////////////////////////////


/// NCBI_CS_ITERATE base macro tests to see if loop should be entered
// If okay, calls ITERATE for linear STL iteration

#define NCBI_CS_ITERATE(Test, Type, Var, Cont) \
if (! (Test)) {} else ITERATE (Type, Var, Cont)

/// NCBI_NC_ITERATE base macro tests to see if loop should be entered
// If okay, calls ERASE_ITERATE for linear STL iteration

#define NCBI_NC_ITERATE(Test, Type, Var, Cont) \
if (! (Test)) {} else ERASE_ITERATE (Type, Var, Cont)

/// NCBI_SWITCH base macro tests to see if switch should be performed
// If okay, calls switch statement

#define NCBI_SWITCH(Test, Chs) \
if (! (Test)) {} else switch(Chs)


/// FOR_EACH base macro calls NCBI_CS_ITERATE with generated components

#define FOR_EACH(Base, Itr, Var) \
NCBI_CS_ITERATE (Base##_Test(Var), Base##_Type, Itr, Base##_Get(Var))

/// EDIT_EACH base macro calls NCBI_NC_ITERATE with generated components

#define EDIT_EACH(Base, Itr, Var) \
NCBI_NC_ITERATE (Base##_Test(Var), Base##_Type, Itr, Base##_Set(Var))

/// ADD_ITEM base macro

#define ADD_ITEM(Base, Var, Ref) \
(Base##_Set(Var).push_back(Ref))

/// LIST_ERASE_ITEM base macro

#define LIST_ERASE_ITEM(Base, Itr, Var) \
(Base##_Set(Var).erase(Itr))

/// VECTOR_ERASE_ITEM base macro

#define VECTOR_ERASE_ITEM(Base, Itr, Var) \
(VECTOR_ERASE (Itr, Base##_Set(Var)))

/// ITEM_HAS base macro

#define ITEM_HAS(Base, Var) \
(Base##_Test(Var))

/// FIELD_IS_EMPTY base macro

#define FIELD_IS_EMPTY(Base, Var) \
    (Base##_Test(Var) && Base##_Get(Var).empty() )

/// RAW_FIELD_IS_EMPTY base macro

#define RAW_FIELD_IS_EMPTY(Var, Fld) \
    ( (Var).IsSet##Fld() && (Var).Get##Fld().empty() )

/// FIELD_IS_EMPTY_OR_UNSET base macro

#define FIELD_IS_EMPTY_OR_UNSET(Base, Var) \
    ( ! Base##_Test(Var) || Base##_Get(Var).empty() )

/// RAW_FIELD_IS_EMPTY_OR_UNSET macro

#define RAW_FIELD_IS_EMPTY_OR_UNSET(Var, Fld) \
    ( ! (Var).IsSet##Fld() || (Var).Get##Fld().empty() )

/// SET_FIELD_IF_UNSET macro

// (The do-while is just so the user has to put a semi-colon after it)
#define SET_FIELD_IF_UNSET(Var, Fld, Val) \
    do { if( ! (Var).IsSet##Fld() ) { (Var).Set##Fld(Val); ChangeMade(CCleanupChange::eChangeQualifiers); } } while(false)

/// RESET_FIELD_IF_EMPTY base macro

// (The do-while is just so the user has to put a semi-colon after it)
#define REMOVE_IF_EMPTY_FIELD(Base, Var) \
    do { if( FIELD_IS_EMPTY(Base, Var) ) { Base##_Reset(Var); ChangeMade(CCleanupChange::eChangeQualifiers); } } while(false)

/// GET_STRING_OR_BLANK base macro

#define GET_STRING_OR_BLANK(Base, Var) \
    (Base##_Test(Var) ? Base##_Get(Var) : kEmptyStr )

/// CHOICE_IS base macro

#define CHOICE_IS(Base, Var, Chs) \
(Base##_Test(Var) && Base##_Chs(Var) == Chs)


/// SWITCH_ON base macro calls NCBI_SWITCH with generated components

#define SWITCH_ON(Base, Var) \
NCBI_SWITCH (Base##_Test(Var), Base##_Chs(Var))


// is_sorted template

template <class Iter, class Comp>
bool seq_mac_is_sorted(Iter first, Iter last, Comp comp)
{
    if (first == last)
        return true;
    
    Iter next = first;
    for (++next; next != last; first = next, ++next) {
        if (comp(*next, *first))
            return false;
    }
    
    return true;
}


// is_unique template assumes that the container is already sorted

template <class Iterator, class Predicate>
bool seq_mac_is_unique(Iterator iter1, Iterator iter2, Predicate pred)
{
    Iterator prev = iter1;
    if (iter1 == iter2) {
        return true;
    }
    for (++iter1;  iter1 != iter2;  ++iter1, ++prev) {
        if (pred(*iter1, *prev)) {
            return false;
        }
    }
    return true;
}


/// IS_SORTED base macro

#define IS_SORTED(Base, Var, Func) \
((! Base##_Test(Var)) || \
seq_mac_is_sorted (Base##_Set(Var).begin(), \
                   Base##_Set(Var).end(), \
                   Func))

/// DO_LIST_SORT base macro

#define DO_LIST_SORT(Base, Var, Func) \
(Base##_Set(Var).sort (Func))

/// DO_VECTOR_SORT base macro

#define DO_VECTOR_SORT(Base, Var, Func) \
(stable_sort (Base##_Set(Var).begin(), \
              Base##_Set(Var).end(), \
              Func))

/// DO_LIST_SORT_HACK base macro

// This is more complex than some of the others
// to get around the WorkShop compiler's lack of support
// for member template functions.
// This should only be needed when you're sorting
// by a function object rather than a plain function.
#define DO_LIST_SORT_HACK(Base, Var, Func) \
    do { \
        vector< Base##_Type::value_type > vec; \
        copy( Base##_Get(Var).begin(), Base##_Get(Var).end(), back_inserter(vec) ); \
        stable_sort( vec.begin(), vec.end(), Func ); \
        Base##_Set(Var).clear(); \
        copy( vec.begin(), vec.end(), back_inserter(Base##_Set(Var)) ); \
    } while(false) // The purpose of the one-time do-while is to force a semicolon


/// IS_UNIQUE base macro

#define IS_UNIQUE(Base, Var, Func) \
((! Base##_Test(Var)) || \
seq_mac_is_unique (Base##_Set(Var).begin(), \
                   Base##_Set(Var).end(), \
                   Func))

/// DO_UNIQUE base macro

#define DO_UNIQUE(Base, Var, Func) \
{ \
    Base##_Type::iterator it = unique (Base##_Set(Var).begin(), \
                                       Base##_Set(Var).end(), \
                                       Func); \
    it = Base##_Set(Var).erase(it, Base##_Set(Var).end()); \
}

// keeps only the first of all the ones that match
#define UNIQUE_WITHOUT_SORT(Base, Var, FuncType, CleanupChangeType) \
{ \
    if( Base##_Test(Var) ) { \
      set<Base##_Type::value_type, FuncType> valuesAlreadySeen; \
      Base##_Type non_duplicate_items; \
      FOR_EACH(Base, iter, Var ) { \
          if( valuesAlreadySeen.find(*iter) == valuesAlreadySeen.end() ) { \
              non_duplicate_items.push_back( *iter ); \
              valuesAlreadySeen.insert( *iter ); \
          } \
      } \
      if( Base##_Get(Var).size() != non_duplicate_items.size() ) { \
          ChangeMade(CleanupChangeType); \
      } \
      Base##_Set(Var).swap( non_duplicate_items ); \
    } \
}


// "FOR_EACH_XXX_ON_YYY" does a linear const traversal of STL containers
// "EDIT_EACH_XXX_ON_YYY" does a linear non-const traversal of STL containers

// "SWITCH_ON_XXX_CHOICE" switches on the item subtype

// "ADD_XXX_TO_YYY" adds an element to a specified object
// "ERASE_XXX_ON_YYY" deletes a specified object within an iterator

// Miscellaneous macros for testing objects include
// "XXX_IS_YYY" or "XXX_HAS_YYY"
// "XXX_CHOICE_IS"


///
/// list <string> macros

/// STRING_IN_LIST macros

#define STRING_IN_LIST_Type      list <string>
#define STRING_IN_LIST_Test(Var) (! (Var).empty())
#define STRING_IN_LIST_Get(Var)  (Var)
#define STRING_IN_LIST_Set(Var)  (Var)

/// LIST_HAS_STRING

#define LIST_HAS_STRING(Var) \
ITEM_HAS (STRING_IN_LIST, Var)

/// FOR_EACH_STRING_IN_LIST
/// EDIT_EACH_STRING_IN_LIST
// list <string>& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_STRING_IN_LIST(Itr, Var) \
FOR_EACH (STRING_IN_LIST, Itr, Var)

#define EDIT_EACH_STRING_IN_LIST(Itr, Var) \
EDIT_EACH (STRING_IN_LIST, Itr, Var)

/// ADD_STRING_TO_LIST

#define ADD_STRING_TO_LIST(Var, Ref) \
ADD_ITEM (STRING_IN_LIST, Var, Ref)

/// ERASE_STRING_IN_LIST

#define ERASE_STRING_IN_LIST(Itr, Var) \
LIST_ERASE_ITEM (STRING_IN_LIST, Itr, Var)

/// STRING_IN_LIST_IS_SORTED

#define STRING_IN_LIST_IS_SORTED(Var, Func) \
IS_SORTED (STRING_IN_LIST, Var, Func)

/// SORT_STRING_IN_LIST

#define SORT_STRING_IN_LIST(Var, Func) \
DO_LIST_SORT (STRING_IN_LIST, Var, Func)

/// STRING_IN_LIST_IS_UNIQUE

#define STRING_IN_LIST_IS_UNIQUE(Var, Func) \
IS_UNIQUE (STRING_IN_LIST, Var, Func)

/// UNIQUE_STRING_IN_LIST

#define UNIQUE_STRING_IN_LIST(Var, Func) \
DO_UNIQUE (STRING_IN_LIST, Var, Func)


///
/// vector <string> macros

/// STRING_IN_VECTOR macros

#define STRING_IN_VECTOR_Type      vector <string>
#define STRING_IN_VECTOR_Test(Var) (! (Var).empty())
#define STRING_IN_VECTOR_Get(Var)  (Var)
#define STRING_IN_VECTOR_Set(Var)  (Var)

/// VECTOR_HAS_STRING

#define VECTOR_HAS_STRING(Var) \
ITEM_HAS (STRING_IN_VECTOR, Var)

/// FOR_EACH_STRING_IN_VECTOR
/// EDIT_EACH_STRING_IN_VECTOR
// vector <string>& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_STRING_IN_VECTOR(Itr, Var) \
FOR_EACH (STRING_IN_VECTOR, Itr, Var)

#define EDIT_EACH_STRING_IN_VECTOR(Itr, Var) \
EDIT_EACH (STRING_IN_VECTOR, Itr, Var)

/// ADD_STRING_TO_VECTOR

#define ADD_STRING_TO_VECTOR(Var, Ref) \
ADD_ITEM (STRING_IN_VECTOR, Var, Ref)

/// ERASE_STRING_IN_VECTOR

#define ERASE_STRING_IN_VECTOR(Itr, Var) \
VECTOR_ERASE_ITEM (STRING_IN_VECTOR, Itr, Var)

/// STRING_IN_VECTOR_IS_SORTED

#define STRING_IN_VECTOR_IS_SORTED(Var, Func) \
IS_SORTED (STRING_IN_VECTOR, Var, Func)

/// SORT_STRING_IN_VECTOR

#define SORT_STRING_IN_VECTOR(Var, Func) \
DO_VECTOR_SORT (STRING_IN_VECTOR, Var, Func)

/// STRING_IN_VECTOR_IS_UNIQUE

#define STRING_IN_VECTOR_IS_UNIQUE(Var, Func) \
IS_UNIQUE (STRING_IN_VECTOR, Var, Func)

/// UNIQUE_STRING_IN_VECTOR

#define UNIQUE_STRING_IN_VECTOR(Var, Func) \
DO_UNIQUE (STRING_IN_VECTOR, Var, Func)


///
/// <string> macros

/// CHAR_IN_STRING macros

#define CHAR_IN_STRING_Type      string
#define CHAR_IN_STRING_Test(Var) (! (Var).empty())
#define CHAR_IN_STRING_Get(Var)  (Var)
#define CHAR_IN_STRING_Set(Var)  (Var)

/// STRING_HAS_CHAR

#define STRING_HAS_CHAR(Var) \
ITEM_HAS (CHAR_IN_STRING, Var)

/// FOR_EACH_CHAR_IN_STRING
/// EDIT_EACH_CHAR_IN_STRING
// string& as input, dereference with [const] char& ch = *itr;

#define FOR_EACH_CHAR_IN_STRING(Itr, Var) \
FOR_EACH (CHAR_IN_STRING, Itr, Var)

#define EDIT_EACH_CHAR_IN_STRING(Itr, Var) \
EDIT_EACH (CHAR_IN_STRING, Itr, Var)

/// ADD_CHAR_TO_STRING

#define ADD_CHAR_TO_STRING(Var, Ref) \
ADD_ITEM (CHAR_IN_STRING, Var, Ref)

/// ERASE_CHAR_IN_STRING

#define ERASE_CHAR_IN_STRING(Itr, Var) \
LIST_ERASE_ITEM (CHAR_IN_STRING, Itr, Var)

/// CHAR_IN_STRING_IS_SORTED

#define CHAR_IN_STRING_IS_SORTED(Var, Func) \
IS_SORTED (CHAR_IN_STRING, Var, Func)

/// SORT_CHAR_IN_STRING

#define SORT_CHAR_IN_STRING(Var, Func) \
DO_LIST_SORT (CHAR_IN_STRING, Var, Func)

/// CHAR_IN_STRING_IS_UNIQUE

#define CHAR_IN_STRING_IS_UNIQUE(Var, Func) \
IS_UNIQUE (CHAR_IN_STRING, Var, Func)

/// UNIQUE_CHAR_IN_STRING

#define UNIQUE_CHAR_IN_STRING(Var, Func) \
DO_UNIQUE (CHAR_IN_STRING, Var, Func)

/// CHAR_IN_STRING_IS_EMPTY

#define CHAR_IN_STRING_IS_EMPTY(Var) \
    FIELD_IS_EMPTY(CHAR_IN_STRING, Var, Func)


///
/// Generic FIELD macros

/// FIELD_IS base macro

#define FIELD_IS(Var, Fld) \
    ((Var).Is##Fld())

/// FIELD_IS_SET_AND_IS base macro

#define FIELD_IS_SET_AND_IS(Var, Fld, Chs) \
    ( (Var).IsSet##Fld() && (Var).Get##Fld().Is##Chs() )

/// FIELD_IS_SET base macro

#define FIELD_IS_SET(Var, Fld) \
    ((Var).IsSet##Fld())

/// FIELD_CHAIN_OF_2_IS_SET

#define FIELD_CHAIN_OF_2_IS_SET(Var, Fld1, Fld2) \
    ( (Var).IsSet##Fld1() && \
      (Var).Get##Fld1().IsSet##Fld2() )

/// FIELD_CHAIN_OF_3_IS_SET

#define FIELD_CHAIN_OF_3_IS_SET(Var, Fld1, Fld2, Fld3) \
    ( (Var).IsSet##Fld1() && \
      (Var).Get##Fld1().IsSet##Fld2() && \
      (Var).Get##Fld1().Get##Fld2().IsSet##Fld3() )

/// FIELD_CHAIN_OF_4_IS_SET

#define FIELD_CHAIN_OF_4_IS_SET(Var, Fld1, Fld2, Fld3, Fld4) \
    ( (Var).IsSet##Fld1() && \
      (Var).Get##Fld1().IsSet##Fld2() && \
      (Var).Get##Fld1().Get##Fld2().IsSet##Fld3() && \
      (Var).Get##Fld1().Get##Fld2().Get##Fld3().IsSet##Fld4() )


/// FIELD_CHAIN_OF_5_IS_SET

#define FIELD_CHAIN_OF_5_IS_SET(Var, Fld1, Fld2, Fld3, Fld4, Fld5) \
    ( (Var).IsSet##Fld1() && \
    (Var).Get##Fld1().IsSet##Fld2() && \
    (Var).Get##Fld1().Get##Fld2().IsSet##Fld3() && \
    (Var).Get##Fld1().Get##Fld2().Get##Fld3().IsSet##Fld4() && \
    (Var).Get##Fld1().Get##Fld2().Get##Fld3().Get##Fld4().IsSet##Fld5() )

/// GET_FIELD base macro

#define GET_FIELD(Var, Fld) \
    ((Var).Get##Fld())

/// GET_MUTABLE base macro

#define GET_MUTABLE(Var, Fld) \
    ((Var).Set##Fld())

/// SET_FIELD base macro

#define SET_FIELD(Var, Fld, Val) \
    ((Var).Set##Fld(Val))

/// RESET_FIELD base macro

#define RESET_FIELD(Var, Fld) \
    ((Var).Reset##Fld())


/// STRING_FIELD_MATCH base macro

#define STRING_FIELD_MATCH(Var, Fld, Str) \
    ((Var).IsSet##Fld() && NStr::EqualNocase((Var).Get##Fld(), Str))

/// STRING_FIELD_MATCH_BUT_ONLY_CASE_INSENSITIVE base macro

#define STRING_FIELD_MATCH_BUT_ONLY_CASE_INSENSITIVE(Var, Fld, Str) \
    ((Var).IsSet##Fld() && NStr::EqualNocase((Var).Get##Fld(), (Str)) && \
        (Var).Get##Fld() != (Str) )

/// STRING_FIELD_CHOICE_MATCH base macro

#define STRING_FIELD_CHOICE_MATCH( Var, Fld, Chs, Value) \
    ( (Var).IsSet##Fld() && (Var).Get##Fld().Is##Chs() && \
      NStr::EqualNocase((Var).Get##Fld().Get##Chs(), (Value)) )


/// GET_STRING_FLD_OR_BLANK base macro

#define GET_STRING_FLD_OR_BLANK(Var, Fld) \
    ( (Var).IsSet##Fld() ? (Var).Get##Fld() : kEmptyStr )

/// STRING_FIELD_NOT_EMPTY base macro

#define STRING_FIELD_NOT_EMPTY(Var, Fld) \
    ( (Var).IsSet##Fld() && ! (Var).Get##Fld().empty() )

/// STRING_SET_MATCH base macro (for list or vectors)

#define STRING_SET_MATCH(Var, Fld, Str) \
    ((Var).IsSet##Fld() && NStr::FindNoCase((Var).Get##Fld(), Str) != NULL)

/// FIELD_OUT_OF_RANGE base macro

#define FIELD_OUT_OF_RANGE(Var, Fld, Lower, Upper) \
    ( (Var).IsSet##Fld() && ( (Var).Get##Fld() < (Lower) || (Var).Get##Fld() > (Upper) ) )

/// FIELD_EQUALS base macro

#define FIELD_EQUALS( Var, Fld, Value ) \
    ( (Var).IsSet##Fld() && (Var).Get##Fld() == (Value) )

/// FIELD_CHOICE_EQUALS base macro

#define FIELD_CHOICE_EQUALS( Var, Fld, Chs, Value) \
    ( (Var).IsSet##Fld() && (Var).Get##Fld().Is##Chs() && \
      (Var).Get##Fld().Get##Chs() == (Value) )

#define FIELD_CHOICE_EMPTY( Var, Fld, Chs) \
    ( ! (Var).IsSet##Fld() || ! (Var).Get##Fld().Is##Chs() || \
      (Var).Get##Fld().Get##Chs().empty() )

/// CALL_IF_SET base macro

#define CALL_IF_SET( Func, Var, Fld ) \
    { \
        if( (Var).IsSet##Fld() ) { \
            Func( GET_MUTABLE( (Var), Fld) ); \
        } \
    }

/// CALL_IF_SET_CHAIN_2 base macro

#define CALL_IF_SET_CHAIN_2( Func, Var, Fld1, Fld2 ) \
    { \
        if( (Var).IsSet##Fld1() ) { \
            CALL_IF_SET( Func, (Var).Set##Fld1(), Fld2 ); \
        } \
    }

/// TEST_FIELD_CHOICE

#define TEST_FIELD_CHOICE( Var, Fld, Chs ) \
    ( (Var).IsSet##Fld() && (Var).Get##Fld().Which() == (Chs) )

///
/// CSeq_submit macros

/// SEQSUBMIT_CHOICE macros

#define SEQSUBMIT_CHOICE_Test(Var) ((Var).IsSetData() && \
                                    (Var).GetData().Which() != CSeq_submit::TData::e_not_set)
#define SEQSUBMIT_CHOICE_Chs(Var)  (Var).GetData().Which()

/// SEQSUBMIT_CHOICE_IS

#define SEQSUBMIT_CHOICE_IS(Var, Chs) \
CHOICE_IS (SEQSUBMIT_CHOICE, Var, Chs)

/// SEQSUBMIT_IS_ENTRYS

#define SEQSUBMIT_IS_ENTRYS(Var) \
SEQSUBMIT_CHOICE_IS (Var, NCBI_SEQSUBMIT(Entrys))

/// SEQSUBMIT_IS_ANNOTS

#define SEQSUBMIT_IS_ANNOTS(Var) \
SEQSUBMIT_CHOICE_IS (Var, NCBI_SEQSUBMIT(Annots))

/// SWITCH_ON_SEQSUBMIT_CHOICE

#define SWITCH_ON_SEQSUBMIT_CHOICE(Var) \
SWITCH_ON (SEQSUBMIT_CHOICE, Var)

/// SEQENTRY_ON_SEQSUBMIT macros

#define SEQENTRY_ON_SEQSUBMIT_Type      CSeq_submit::TData::TEntrys
#define SEQENTRY_ON_SEQSUBMIT_Test(Var) ((Var).IsSetData() && (Var).GetData().IsEntrys())
#define SEQENTRY_ON_SEQSUBMIT_Get(Var)  (Var).GetData().GetEntrys()
#define SEQENTRY_ON_SEQSUBMIT_Set(Var)  (Var).SetData().SetEntrys()

/// FOR_EACH_SEQENTRY_ON_SEQSUBMIT
/// EDIT_EACH_SEQENTRY_ON_SEQSUBMIT
// CSeq_submit& as input, dereference with [const] CSeq_entry& se = **itr;

#define FOR_EACH_SEQENTRY_ON_SEQSUBMIT(Itr, Var) \
FOR_EACH (SEQENTRY_ON_SEQSUBMIT, Itr, Var)

#define EDIT_EACH_SEQENTRY_ON_SEQSUBMIT(Itr, Var) \
EDIT_EACH (SEQENTRY_ON_SEQSUBMIT, Itr, Var)

/// ADD_SEQENTRY_TO_SEQSUBMIT

#define ADD_SEQENTRY_TO_SEQSUBMIT(Var, Ref) \
ADD_ITEM (SEQENTRY_ON_SEQSUBMIT, Var, Ref)

/// ERASE_SEQENTRY_ON_SEQSUBMIT

#define ERASE_SEQENTRY_ON_SEQSUBMIT(Itr, Var) \
LIST_ERASE_ITEM (SEQENTRY_ON_SEQSUBMIT, Itr, Var)


/// SEQANNOT_ON_SEQSUBMIT macros

#define SEQANNOT_ON_SEQSUBMIT_Type      CSeq_submit::TData::TAnnots
#define SEQANNOT_ON_SEQSUBMIT_Test(Var) ((Var).IsSetData() && (Var).GetData().IsAnnots())
#define SEQANNOT_ON_SEQSUBMIT_Get(Var)  (Var).GetData().GetAnnots()
#define SEQANNOT_ON_SEQSUBMIT_Set(Var)  (Var).SetData().SetAnnots()

/// SEQSUBMIT_HAS_SEQANNOT

#define SEQSUBMIT_HAS_SEQANNOT(Var) \
ITEM_HAS (SEQANNOT_ON_SEQSUBMIT, Var)

/// FOR_EACH_SEQANNOT_ON_SEQSUBMIT
/// EDIT_EACH_SEQANNOT_ON_SEQSUBMIT
// CBioseq_set& as input, dereference with [const] CSeq_annot& annot = **itr;

#define FOR_EACH_SEQANNOT_ON_SEQSUBMIT(Itr, Var) \
FOR_EACH (SEQANNOT_ON_SEQSUBMIT, Itr, Var)

#define EDIT_EACH_SEQANNOT_ON_SEQSUBMIT(Itr, Var) \
EDIT_EACH (SEQANNOT_ON_SEQSUBMIT, Itr, Var)

/// ADD_SEQANNOT_TO_SEQSUBMIT

#define ADD_SEQANNOT_TO_SEQSUBMIT(Var, Ref) \
ADD_ITEM (SEQANNOT_ON_SEQSUBMIT, Var, Ref)

/// ERASE_SEQANNOT_ON_SEQSUBMIT

#define ERASE_SEQANNOT_ON_SEQSUBMIT(Itr, Var) \
LIST_ERASE_ITEM (SEQANNOT_ON_SEQSUBMIT, Itr, Var)


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

////
//// FEATID_ON_SEQFEAT macros
//// ( Warning: features also have an "Id" field (deprecated?) )

#define FEATID_ON_BIOSEQ_Type      CSeq_feat::TIds:
#define FEATID_ON_BIOSEQ_Test(Var) (Var).IsSetIds()
#define FEATID_ON_BIOSEQ_Get(Var)  (Var).GetIds()
#define FEATID_ON_BIOSEQ_Set(Var)  (Var).SetIds()

#define EDIT_EACH_FEATID_ON_SEQFEAT( Iter, Var ) \
    EDIT_EACH( FEATID_ON_BIOSEQ, Iter, Var )

///
/// CSeq_id macros

/// SEQID_CHOICE macros

#define SEQID_CHOICE_Test(Var) (Var).Which() != CSeq_id::e_not_set
#define SEQID_CHOICE_Chs(Var)  (Var).Which()

/// SEQID_CHOICE_IS

#define SEQID_CHOICE_IS(Var, Chs) \
CHOICE_IS (SEQID_CHOICE, Var, Chs)

/// SWITCH_ON_SEQID_CHOICE

#define SWITCH_ON_SEQID_CHOICE(Var) \
SWITCH_ON (SEQID_CHOICE, Var)


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

/// BOUND_ON_SEQALIGN macros

#define BOUND_ON_SEQALIGN_Type      CSeq_align::TBounds
#define BOUND_ON_SEQALIGN_Test(Var) (Var).IsSetBounds()
#define BOUND_ON_SEQALIGN_Get(Var)  (Var).GetBounds()
#define BOUND_ON_SEQALIGN_Set(Var)  (Var).SetBounds()

// EDIT_EACH_BOUND_ON_SEQALIGN

#define EDIT_EACH_BOUND_ON_SEQALIGN(Itr, Var) \
    EDIT_EACH (BOUND_ON_SEQALIGN, Itr, Var)

/// SEGTYPE_ON_SEQALIGN macros

#define SEGTYPE_ON_SEQALIGN_Test(Var) ((Var).IsSetSegs())
#define SEGTYPE_ON_SEQALIGN_Chs(Var)  (Var).GetSegs().Which()

#define SWITCH_ON_SEGTYPE_ON_SEQALIGN(Var) \
    SWITCH_ON( SEGTYPE_ON_SEQALIGN, Var )

/// DENDIAG_ON_SEQALIGN macros

#define DENDIAG_ON_SEQALIGN_Type        CSeq_align_Base::C_Segs::TDendiag
#define DENDIAG_ON_SEQALIGN_Test(Var)   (Var).IsSetSegs() && (Var).GetSegs().IsDendiag()
#define DENDIAG_ON_SEQALIGN_Get(Var)    (Var).GetSegs().GetDendiag()
#define DENDIAG_ON_SEQALIGN_Set(Var)    (Var).SetSegs().SetDendiag()

/// EDIT_EACH_DENDIAG_ON_SEQALIGN

#define EDIT_EACH_DENDIAG_ON_SEQALIGN(Itr, Var) \
EDIT_EACH (DENDIAG_ON_SEQALIGN, Itr, Var)

/// STDSEG_ON_SEQALIGN macros

#define STDSEG_ON_SEQALIGN_Type        CSeq_align_Base::C_Segs::TStd
#define STDSEG_ON_SEQALIGN_Test(Var)   (Var).IsSetSegs() && (Var).GetSegs().IsStd()
#define STDSEG_ON_SEQALIGN_Get(Var)    (Var).GetSegs().GetStd()
#define STDSEG_ON_SEQALIGN_Set(Var)    (Var).SetSegs().SetStd()

/// EDIT_EACH_STDSEG_ON_SEQALIGN

#define EDIT_EACH_STDSEG_ON_SEQALIGN(Itr, Var) \
EDIT_EACH (STDSEG_ON_SEQALIGN, Itr, Var)

/// RECURSIVE_SEQALIGN_ON_SEQALIGN macros

#define RECURSIVE_SEQALIGN_ON_SEQALIGN_Type        CSeq_align_Base::C_Segs::TDisc::Tdata
#define RECURSIVE_SEQALIGN_ON_SEQALIGN_Test(Var)   (Var).IsSetSegs() && (Var).GetSegs().IsDisc()
#define RECURSIVE_SEQALIGN_ON_SEQALIGN_Get(Var)    (Var).GetSegs().GetDisc().Get()
#define RECURSIVE_SEQALIGN_ON_SEQALIGN_Set(Var)    (Var).SetSegs().SetDisc().Set()

/// EDIT_EACH_RECURSIVE_SEQALIGN_ON_SEQALIGN

#define EDIT_EACH_RECURSIVE_SEQALIGN_ON_SEQALIGN(Itr, Var) \
EDIT_EACH (RECURSIVE_SEQALIGN_ON_SEQALIGN, Itr, Var)

/// SEQID_ON_DENDIAG macros

#define SEQID_ON_DENDIAG_Type        CDense_diag_Base::TIds
#define SEQID_ON_DENDIAG_Test(Var)   (Var).IsSetIds()
#define SEQID_ON_DENDIAG_Get(Var)    (Var).GetIds()
#define SEQID_ON_DENDIAG_Set(Var)    (Var).SetIds()

/// EDIT_EACH_SEQID_ON_DENDIAG

#define EDIT_EACH_SEQID_ON_DENDIAG(Itr, Var) \
EDIT_EACH (SEQID_ON_DENDIAG, Itr, Var)

/// SEQID_ON_DENSEG macros

#define SEQID_ON_DENSEG_Type        CDense_seg::TIds
#define SEQID_ON_DENSEG_Test(Var)   (Var).IsSetIds()
#define SEQID_ON_DENSEG_Get(Var)    (Var).GetIds()
#define SEQID_ON_DENSEG_Set(Var)    (Var).SetIds()

/// EDIT_EACH_SEQID_ON_DENSEG

#define EDIT_EACH_SEQID_ON_DENSEG(Itr, Var) \
EDIT_EACH (SEQID_ON_DENSEG, Itr, Var)

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
/// CBioSource macros

/// BIOSOURCE_GENOME macros

#define BIOSOURCE_GENOME_Test(Var) (Var).IsSetGenome()
#define BIOSOURCE_GENOME_Chs(Var)  (Var).GetGenome()

/// BIOSOURCE_GENOME_IS

#define BIOSOURCE_GENOME_IS(Var, Chs) \
CHOICE_IS (BIOSOURCE_GENOME, Var, Chs)

/// SWITCH_ON_BIOSOURCE_GENOME

#define SWITCH_ON_BIOSOURCE_GENOME(Var) \
SWITCH_ON (BIOSOURCE_GENOME, Var)


/// BIOSOURCE_ORIGIN macros

#define BIOSOURCE_ORIGIN_Test(Var) (Var).IsSetOrigin()
#define BIOSOURCE_ORIGIN_Chs(Var)  (Var).GetOrigin()

/// BIOSOURCE_ORIGIN_IS

#define BIOSOURCE_ORIGIN_IS(Var, Chs) \
CHOICE_IS (BIOSOURCE_ORIGIN, Var, Chs)

/// SWITCH_ON_BIOSOURCE_ORIGIN

#define SWITCH_ON_BIOSOURCE_ORIGIN(Var) \
SWITCH_ON (BIOSOURCE_ORIGIN, Var)


/// ORGREF_ON_BIOSOURCE macros

#define ORGREF_ON_BIOSOURCE_Test(Var) (Var).IsSetOrg()

/// BIOSOURCE_HAS_ORGREF

#define BIOSOURCE_HAS_ORGREF(Var) \
ITEM_HAS (ORGREF_ON_BIOSOURCE, Var)


/// ORGNAME_ON_BIOSOURCE macros

#define ORGNAME_ON_BIOSOURCE_Test(Var) (Var).IsSetOrgname()

/// BIOSOURCE_HAS_ORGNAME

#define BIOSOURCE_HAS_ORGNAME(Var) \
ITEM_HAS (ORGNAME_ON_BIOSOURCE, Var)


/// SUBSOURCE_ON_BIOSOURCE macros

#define SUBSOURCE_ON_BIOSOURCE_Type       CBioSource::TSubtype
#define SUBSOURCE_ON_BIOSOURCE_Test(Var)  (Var).IsSetSubtype()
#define SUBSOURCE_ON_BIOSOURCE_Get(Var)   (Var).GetSubtype()
#define SUBSOURCE_ON_BIOSOURCE_Set(Var)   (Var).SetSubtype()
#define SUBSOURCE_ON_BIOSOURCE_Reset(Var) (Var).ResetSubtype()

/// BIOSOURCE_HAS_SUBSOURCE

#define BIOSOURCE_HAS_SUBSOURCE(Var) \
ITEM_HAS (SUBSOURCE_ON_BIOSOURCE, Var)

/// FOR_EACH_SUBSOURCE_ON_BIOSOURCE
/// EDIT_EACH_SUBSOURCE_ON_BIOSOURCE
// CBioSource& as input, dereference with [const] CSubSource& sbs = **itr

#define FOR_EACH_SUBSOURCE_ON_BIOSOURCE(Itr, Var) \
FOR_EACH (SUBSOURCE_ON_BIOSOURCE, Itr, Var)

#define EDIT_EACH_SUBSOURCE_ON_BIOSOURCE(Itr, Var) \
EDIT_EACH (SUBSOURCE_ON_BIOSOURCE, Itr, Var)

/// ADD_SUBSOURCE_TO_BIOSOURCE

#define ADD_SUBSOURCE_TO_BIOSOURCE(Var, Ref) \
ADD_ITEM (SUBSOURCE_ON_BIOSOURCE, Var, Ref)

/// ERASE_SUBSOURCE_ON_BIOSOURCE

#define ERASE_SUBSOURCE_ON_BIOSOURCE(Itr, Var) \
LIST_ERASE_ITEM (SUBSOURCE_ON_BIOSOURCE, Itr, Var)

/// SUBSOURCE_ON_BIOSOURCE_IS_SORTED

#define SUBSOURCE_ON_BIOSOURCE_IS_SORTED(Var, Func) \
IS_SORTED (SUBSOURCE_ON_BIOSOURCE, Var, Func)

/// SORT_SUBSOURCE_ON_BIOSOURCE

#define SORT_SUBSOURCE_ON_BIOSOURCE(Var, Func) \
DO_LIST_SORT (SUBSOURCE_ON_BIOSOURCE, Var, Func)

/// SUBSOURCE_ON_BIOSOURCE_IS_UNIQUE

#define SUBSOURCE_ON_BIOSOURCE_IS_UNIQUE(Var, Func) \
IS_UNIQUE (SUBSOURCE_ON_BIOSOURCE, Var, Func)

/// UNIQUE_SUBSOURCE_ON_BIOSOURCE

#define UNIQUE_SUBSOURCE_ON_BIOSOURCE(Var, Func) \
DO_UNIQUE (SUBSOURCE_ON_BIOSOURCE, Var, Func)

/// REMOVE_IF_EMPTY_SUBSOURCE_ON_BIOSOURCE

#define REMOVE_IF_EMPTY_SUBSOURCE_ON_BIOSOURCE(Var) \
    REMOVE_IF_EMPTY_FIELD(SUBSOURCE_ON_BIOSOURCE, Var)

/// ORGMOD_ON_BIOSOURCE macros

#define ORGMOD_ON_BIOSOURCE_Type      COrgName::TMod
#define ORGMOD_ON_BIOSOURCE_Test(Var) (Var).IsSetOrgMod()
#define ORGMOD_ON_BIOSOURCE_Get(Var)  (Var).GetOrgname().GetMod()
#define ORGMOD_ON_BIOSOURCE_Set(Var)  (Var).SetOrg().SetOrgname().SetMod()

/// BIOSOURCE_HAS_ORGMOD

#define BIOSOURCE_HAS_ORGMOD(Var) \
ITEM_HAS (ORGMOD_ON_BIOSOURCE, Var)

/// FOR_EACH_ORGMOD_ON_BIOSOURCE
/// EDIT_EACH_ORGMOD_ON_BIOSOURCE
// CBioSource& as input, dereference with [const] COrgMod& omd = **itr

#define FOR_EACH_ORGMOD_ON_BIOSOURCE(Itr, Var) \
FOR_EACH (ORGMOD_ON_BIOSOURCE, Itr, Var)

#define EDIT_EACH_ORGMOD_ON_BIOSOURCE(Itr, Var) \
EDIT_EACH (ORGMOD_ON_BIOSOURCE, Itr, Var)

/// ADD_ORGMOD_TO_BIOSOURCE

#define ADD_ORGMOD_TO_BIOSOURCE(Var, Ref) \
ADD_ITEM (ORGMOD_ON_BIOSOURCE, Var, Ref)

/// ERASE_ORGMOD_ON_BIOSOURCE

#define ERASE_ORGMOD_ON_BIOSOURCE(Itr, Var) \
LIST_ERASE_ITEM (ORGMOD_ON_BIOSOURCE, Itr, Var)

/// ORGMOD_ON_BIOSOURCE_IS_SORTED

#define ORGMOD_ON_BIOSOURCE_IS_SORTED(Var, Func) \
IS_SORTED (ORGMOD_ON_BIOSOURCE, Var, Func)

/// SORT_ORGMOD_ON_BIOSOURCE

#define SORT_ORGMOD_ON_BIOSOURCE(Var, Func) \
DO_LIST_SORT (ORGMOD_ON_BIOSOURCE, Var, Func)

/// ORGMOD_ON_BIOSOURCE_IS_UNIQUE

#define ORGMOD_ON_BIOSOURCE_IS_UNIQUE(Var, Func) \
IS_UNIQUE (ORGMOD_ON_BIOSOURCE, Var, Func)

/// UNIQUE_ORGMOD_ON_BIOSOURCE

#define UNIQUE_ORGMOD_ON_BIOSOURCE(Var, Func) \
DO_UNIQUE (ORGMOD_ON_BIOSOURCE, Var, Func)


///
/// COrg_ref macros

/// ORGMOD_ON_ORGREF macros

#define ORGMOD_ON_ORGREF_Type      COrgName::TMod
#define ORGMOD_ON_ORGREF_Test(Var) (Var).IsSetOrgMod()
#define ORGMOD_ON_ORGREF_Get(Var)  (Var).GetOrgname().GetMod()
#define ORGMOD_ON_ORGREF_Set(Var)  (Var).SetOrgname().SetMod()

/// ORGREF_HAS_ORGMOD

#define ORGREF_HAS_ORGMOD(Var) \
ITEM_HAS (ORGMOD_ON_ORGREF, Var)

/// FOR_EACH_ORGMOD_ON_ORGREF
/// EDIT_EACH_ORGMOD_ON_ORGREF
// COrg_ref& as input, dereference with [const] COrgMod& omd = **itr

#define FOR_EACH_ORGMOD_ON_ORGREF(Itr, Var) \
FOR_EACH (ORGMOD_ON_ORGREF, Itr, Var)

#define EDIT_EACH_ORGMOD_ON_ORGREF(Itr, Var) \
EDIT_EACH (ORGMOD_ON_ORGREF, Itr, Var)

/// ADD_ORGMOD_TO_ORGREF

#define ADD_ORGMOD_TO_ORGREF(Var, Ref) \
ADD_ITEM (ORGMOD_ON_ORGREF, Var, Ref)

/// ERASE_ORGMOD_ON_ORGREF

#define ERASE_ORGMOD_ON_ORGREF(Itr, Var) \
LIST_ERASE_ITEM (ORGMOD_ON_ORGREF, Itr, Var)

/// ORGMOD_ON_ORGREF_IS_SORTED

#define ORGMOD_ON_ORGREF_IS_SORTED(Var, Func) \
IS_SORTED (ORGMOD_ON_ORGREF, Var, Func)

/// SORT_ORGMOD_ON_ORGREF

#define SORT_ORGMOD_ON_ORGREF(Var, Func) \
DO_LIST_SORT (ORGMOD_ON_ORGREF, Var, Func)

/// ORGMOD_ON_ORGREF_IS_UNIQUE

#define ORGMOD_ON_ORGREF_IS_UNIQUE(Var, Func) \
IS_UNIQUE (ORGMOD_ON_ORGREF, Var, Func)

/// UNIQUE_ORGMOD_ON_ORGREF

#define UNIQUE_ORGMOD_ON_ORGREF(Var, Func) \
DO_UNIQUE (ORGMOD_ON_ORGREF, Var, Func)


/// DBXREF_ON_ORGREF macros

#define DBXREF_ON_ORGREF_Type      COrg_ref::TDb
#define DBXREF_ON_ORGREF_Test(Var) (Var).IsSetDb()
#define DBXREF_ON_ORGREF_Get(Var)  (Var).GetDb()
#define DBXREF_ON_ORGREF_Set(Var)  (Var).SetDb()

/// ORGREF_HAS_DBXREF

#define ORGREF_HAS_DBXREF(Var) \
ITEM_HAS (DBXREF_ON_ORGREF, Var)

/// FOR_EACH_DBXREF_ON_ORGREF
/// EDIT_EACH_DBXREF_ON_ORGREF
// COrg_ref& as input, dereference with [const] CDbtag& dbt = **itr

#define FOR_EACH_DBXREF_ON_ORGREF(Itr, Var) \
FOR_EACH (DBXREF_ON_ORGREF, Itr, Var)

#define EDIT_EACH_DBXREF_ON_ORGREF(Itr, Var) \
EDIT_EACH (DBXREF_ON_ORGREF, Itr, Var)

/// ADD_DBXREF_TO_ORGREF

#define ADD_DBXREF_TO_ORGREF(Var, Ref) \
ADD_ITEM (DBXREF_ON_ORGREF, Var, Ref)

/// ERASE_DBXREF_ON_ORGREF

#define ERASE_DBXREF_ON_ORGREF(Itr, Var) \
VECTOR_ERASE_ITEM (DBXREF_ON_ORGREF, Itr, Var)

/// DBXREF_ON_ORGREF_IS_SORTED

#define DBXREF_ON_ORGREF_IS_SORTED(Var, Func) \
IS_SORTED (DBXREF_ON_ORGREF, Var, Func)

/// SORT_DBXREF_ON_ORGREF

#define SORT_DBXREF_ON_ORGREF(Var, Func) \
DO_VECTOR_SORT (DBXREF_ON_ORGREF, Var, Func)

/// DBXREF_ON_ORGREF_IS_UNIQUE

#define DBXREF_ON_ORGREF_IS_UNIQUE(Var, Func) \
IS_UNIQUE (DBXREF_ON_ORGREF, Var, Func)

/// UNIQUE_DBXREF_ON_ORGREF

#define UNIQUE_DBXREF_ON_ORGREF(Var, Func) \
DO_UNIQUE (DBXREF_ON_ORGREF, Var, Func)


/// MOD_ON_ORGREF macros

#define MOD_ON_ORGREF_Type      COrg_ref::TMod
#define MOD_ON_ORGREF_Test(Var) (Var).IsSetMod()
#define MOD_ON_ORGREF_Get(Var)  (Var).GetMod()
#define MOD_ON_ORGREF_Set(Var)  (Var).SetMod()

/// ORGREF_HAS_MOD

#define ORGREF_HAS_MOD(Var) \
ITEM_HAS (MOD_ON_ORGREF, Var)

/// FOR_EACH_MOD_ON_ORGREF
/// EDIT_EACH_MOD_ON_ORGREF
// COrg_ref& as input, dereference with [const] string& str = *itr

#define FOR_EACH_MOD_ON_ORGREF(Itr, Var) \
FOR_EACH (MOD_ON_ORGREF, Itr, Var)

#define EDIT_EACH_MOD_ON_ORGREF(Itr, Var) \
EDIT_EACH (MOD_ON_ORGREF, Itr, Var)

/// ERASE_MOD_ON_ORGREF

#define ERASE_MOD_ON_ORGREF(Itr, Var) \
LIST_ERASE_ITEM (MOD_ON_ORGREF, Itr, Var)

#define MOD_ON_ORGREF_IS_EMPTY(Var) \
FIELD_IS_EMPTY( MOD_ON_ORGREF, Var )


/// SYN_ON_ORGREF macros

#define SYN_ON_ORGREF_Type      COrg_ref::TSyn
#define SYN_ON_ORGREF_Test(Var) (Var).IsSetSyn()
#define SYN_ON_ORGREF_Get(Var)  (Var).GetSyn()
#define SYN_ON_ORGREF_Set(Var)  (Var).SetSyn()

/// ORGREF_HAS_SYN

#define ORGREF_HAS_SYN(Var) \
ITEM_HAS (SYN_ON_ORGREF, Var)

/// FOR_EACH_SYN_ON_ORGREF
/// EDIT_EACH_SYN_ON_ORGREF
// COrg_ref& as input, dereference with [const] string& str = *itr

#define FOR_EACH_SYN_ON_ORGREF(Itr, Var) \
FOR_EACH (SYN_ON_ORGREF, Itr, Var)

#define EDIT_EACH_SYN_ON_ORGREF(Itr, Var) \
EDIT_EACH (SYN_ON_ORGREF, Itr, Var)

/// ERASE_SYN_ON_ORGREF

#define ERASE_SYN_ON_ORGREF(Itr, Var) \
LIST_ERASE_ITEM (SYN_ON_ORGREF, Itr, Var)

/// SYN_ON_ORGREF_IS_SORTED

#define SYN_ON_ORGREF_IS_SORTED(Var, Func) \
IS_SORTED (SYN_ON_ORGREF, Var, Func)

/// SORT_SYN_ON_ORGREF

#define SORT_SYN_ON_ORGREF(Var, Func) \
DO_LIST_SORT (SYN_ON_ORGREF, Var, Func)

/// SYN_ON_ORGREF_IS_UNIQUE

#define SYN_ON_ORGREF_IS_UNIQUE(Var, Func) \
IS_UNIQUE (SYN_ON_ORGREF, Var, Func)

/// UNIQUE_SYN_ON_ORGREF

#define UNIQUE_SYN_ON_ORGREF(Var, Func) \
DO_UNIQUE (SYN_ON_ORGREF, Var, Func)

///
/// COrgName macros

/// ORGNAME_CHOICE macros

#define ORGNAME_CHOICE_Test(Var) (Var).IsSetName() && \
                                     (Var).GetName().Which() != COrgName::e_not_set
#define ORGNAME_CHOICE_Chs(Var)  (Var).GetName().Which()

/// ORGNAME_CHOICE_IS

#define ORGNAME_CHOICE_IS(Var, Chs) \
CHOICE_IS (ORGNAME_CHOICE, Var, Chs)

/// SWITCH_ON_ORGNAME_CHOICE

#define SWITCH_ON_ORGNAME_CHOICE(Var) \
SWITCH_ON (ORGNAME_CHOICE, Var)


/// ORGMOD_ON_ORGNAME macros

#define ORGMOD_ON_ORGNAME_Type       COrgName::TMod
#define ORGMOD_ON_ORGNAME_Test(Var)  (Var).IsSetMod()
#define ORGMOD_ON_ORGNAME_Get(Var)   (Var).GetMod()
#define ORGMOD_ON_ORGNAME_Set(Var)   (Var).SetMod()
#define ORGMOD_ON_ORGNAME_Reset(Var) (Var).ResetMod()

/// ORGNAME_HAS_ORGMOD

#define ORGNAME_HAS_ORGMOD(Var) \
ITEM_HAS (ORGMOD_ON_ORGNAME, Var)

/// FOR_EACH_ORGMOD_ON_ORGNAME
/// EDIT_EACH_ORGMOD_ON_ORGNAME
// COrgName& as input, dereference with [const] COrgMod& omd = **itr

#define FOR_EACH_ORGMOD_ON_ORGNAME(Itr, Var) \
FOR_EACH (ORGMOD_ON_ORGNAME, Itr, Var)

#define EDIT_EACH_ORGMOD_ON_ORGNAME(Itr, Var) \
EDIT_EACH (ORGMOD_ON_ORGNAME, Itr, Var)

/// ADD_ORGMOD_TO_ORGNAME

#define ADD_ORGMOD_TO_ORGNAME(Var, Ref) \
ADD_ITEM (ORGMOD_ON_ORGNAME, Var, Ref)

/// ERASE_ORGMOD_ON_ORGNAME

#define ERASE_ORGMOD_ON_ORGNAME(Itr, Var) \
LIST_ERASE_ITEM (ORGMOD_ON_ORGNAME, Itr, Var)

/// ORGMOD_ON_ORGNAME_IS_SORTED

#define ORGMOD_ON_ORGNAME_IS_SORTED(Var, Func) \
IS_SORTED (ORGMOD_ON_ORGNAME, Var, Func)

/// SORT_ORGMOD_ON_ORGNAME

#define SORT_ORGMOD_ON_ORGNAME(Var, Func) \
DO_LIST_SORT (ORGMOD_ON_ORGNAME, Var, Func)

/// ORGMOD_ON_ORGNAME_IS_UNIQUE

#define ORGMOD_ON_ORGNAME_IS_UNIQUE(Var, Func) \
IS_UNIQUE (ORGMOD_ON_ORGNAME, Var, Func)

/// UNIQUE_ORGMOD_ON_ORGNAME

#define UNIQUE_ORGMOD_ON_ORGNAME(Var, Func) \
DO_UNIQUE (ORGMOD_ON_ORGNAME, Var, Func)

#define REMOVE_IF_EMPTY_ORGMOD_ON_ORGNAME(Var) \
REMOVE_IF_EMPTY_FIELD(ORGMOD_ON_ORGNAME, Var)

///
/// CSubSource macros

/// SUBSOURCE_CHOICE macros

#define SUBSOURCE_CHOICE_Test(Var) (Var).IsSetSubtype()
#define SUBSOURCE_CHOICE_Chs(Var)  (Var).GetSubtype()

/// SUBSOURCE_CHOICE_IS

#define SUBSOURCE_CHOICE_IS(Var, Chs) \
CHOICE_IS (SUBSOURCE_CHOICE, Var, Chs)

/// SWITCH_ON_SUBSOURCE_CHOICE

#define SWITCH_ON_SUBSOURCE_CHOICE(Var) \
SWITCH_ON (SUBSOURCE_CHOICE, Var)


///
/// COrgMod macros

/// ORGMOD_CHOICE macros

#define ORGMOD_CHOICE_Test(Var) (Var).IsSetSubtype()
#define ORGMOD_CHOICE_Chs(Var)  (Var).GetSubtype()

/// ORGMOD_CHOICE_IS

#define ORGMOD_CHOICE_IS(Var, Chs) \
CHOICE_IS (ORGMOD_CHOICE, Var, Chs)

/// SWITCH_ON_ORGMOD_CHOICE

#define SWITCH_ON_ORGMOD_CHOICE(Var) \
SWITCH_ON (ORGMOD_CHOICE, Var)

/// ATTRIB_ON_ORGMOD macros

#define ATTRIB_ON_ORGMOD_Test(Var) (Var).IsSetAttrib()
#define ATTRIB_ON_ORGMOD_Get(Var)  (Var).GetAttrib()

/// 
#define GET_ATTRIB_OR_BLANK(Var) \
    GET_STRING_OR_BLANK( ATTRIB_ON_ORGMOD, Var )

///
/// CPubequiv macros

/// PUB_ON_PUBEQUIV macros

#define PUB_ON_PUBEQUIV_Type      CPub_equiv::Tdata
#define PUB_ON_PUBEQUIV_Test(Var) (Var).IsSet()
#define PUB_ON_PUBEQUIV_Get(Var)  (Var).Get()
#define PUB_ON_PUBEQUIV_Set(Var)  (Var).Set()

#define FOR_EACH_PUB_ON_PUBEQUIV(Itr, Var) \
FOR_EACH (PUB_ON_PUBEQUIV, Itr, Var)

#define EDIT_EACH_PUB_ON_PUBEQUIV(Itr, Var) \
EDIT_EACH (PUB_ON_PUBEQUIV, Itr, Var)

/// ADD_PUB_TO_PUBEQUIV

#define ADD_PUB_TO_PUBEQUIV(Var, Ref) \
ADD_ITEM (PUB_ON_PUBEQUIV, Var, Ref)

/// ERASE_PUB_ON_PUBEQUIV

#define ERASE_PUB_ON_PUBEQUIV(Itr, Var) \
LIST_ERASE_ITEM (PUB_ON_PUBEQUIV, Itr, Var)


///
/// CPubdesc macros

/// PUB_ON_PUBDESC macros

#define PUB_ON_PUBDESC_Type      CPub_equiv::Tdata
#define PUB_ON_PUBDESC_Test(Var) (Var).IsSetPub() && (Var).GetPub().IsSet()
#define PUB_ON_PUBDESC_Get(Var)  (Var).GetPub().Get()
#define PUB_ON_PUBDESC_Set(Var)  (Var).SetPub().Set()

/// PUBDESC_HAS_PUB

#define PUBDESC_HAS_PUB(Var) \
ITEM_HAS (PUB_ON_PUBDESC, Var)

/*
#define PUBDESC_HAS_PUB(Pbd) \
((Pbd).IsSetPub())
*/

/// FOR_EACH_PUB_ON_PUBDESC
/// EDIT_EACH_PUB_ON_PUBDESC
// CPubdesc& as input, dereference with [const] CPub& pub = **itr;

#define FOR_EACH_PUB_ON_PUBDESC(Itr, Var) \
FOR_EACH (PUB_ON_PUBDESC, Itr, Var)

#define EDIT_EACH_PUB_ON_PUBDESC(Itr, Var) \
EDIT_EACH (PUB_ON_PUBDESC, Itr, Var)

/// ADD_PUB_TO_PUBDESC

#define ADD_PUB_TO_PUBDESC(Var, Ref) \
ADD_ITEM (PUB_ON_PUBDESC, Var, Ref)

/// ERASE_PUB_ON_PUBDESC

#define ERASE_PUB_ON_PUBDESC(Itr, Var) \
LIST_ERASE_ITEM (PUB_ON_PUBDESC, Itr, Var)


///
/// CPub macros

/// AUTHOR_ON_PUB macros

#define AUTHOR_ON_PUB_Type      CAuth_list::C_Names::TStd
#define AUTHOR_ON_PUB_Test(Var) (Var).IsSetAuthors() && \
                                    (Var).GetAuthors().IsSetNames() && \
                                    (Var).GetAuthors().GetNames().IsStd()
#define AUTHOR_ON_PUB_Get(Var)  (Var).GetAuthors().GetNames().GetStd()
#define AUTHOR_ON_PUB_Set(Var)  (Var).SetAuthors().SetNames().SetStd()

/// PUB_HAS_AUTHOR

#define PUB_HAS_AUTHOR(Var) \
ITEM_HAS (AUTHOR_ON_PUB, Var)

/// FOR_EACH_AUTHOR_ON_PUB
/// EDIT_EACH_AUTHOR_ON_PUB
// CPub& as input, dereference with [const] CAuthor& auth = **itr;

#define FOR_EACH_AUTHOR_ON_PUB(Itr, Var) \
FOR_EACH (AUTHOR_ON_PUB, Itr, Var)

#define EDIT_EACH_AUTHOR_ON_PUB(Itr, Var) \
EDIT_EACH (AUTHOR_ON_PUB, Itr, Var)

/// ADD_AUTHOR_TO_PUB

#define ADD_AUTHOR_TO_PUB(Var, Ref) \
ADD_ITEM (AUTHOR_ON_PUB, Var, Ref)

/// ERASE_AUTHOR_ON_PUB

#define ERASE_AUTHOR_ON_PUB(Itr, Var) \
LIST_ERASE_ITEM (AUTHOR_ON_PUB, Itr, Var)

/// 
/// CCit_art macros

#define ARTICLEID_ON_CITART_Type      CCit_art::TIds::Tdata
#define ARTICLEID_ON_CITART_Test(Var) ( (Var).IsSetIds() && (Var).GetIds().IsSet() )
#define ARTICLEID_ON_CITART_Get(Var)  (Var).GetIds().Get()
#define ARTICLEID_ON_CITART_Set(Var)  (Var).SetIds().Set()

#define FOR_EACH_ARTICLEID_ON_CITART(Itr, Var) \
FOR_EACH (ARTICLEID_ON_CITART, Itr, Var)

///
/// CAuth_list macros

#define AUTHOR_ON_AUTHLIST_Type      CAuth_list::C_Names::TStd
#define AUTHOR_ON_AUTHLIST_Test(Var) (Var).IsSetNames() && \
                                     (Var).GetNames().IsStd()
#define AUTHOR_ON_AUTHLIST_Get(Var)  (Var).GetNames().GetStd()
#define AUTHOR_ON_AUTHLIST_Set(Var)  (Var).SetNames().SetStd()

#define EDIT_EACH_AUTHOR_ON_AUTHLIST(Itr, Var) \
EDIT_EACH (AUTHOR_ON_AUTHLIST, Itr, Var)

/// ERASE_AUTHOR_ON_AUTHLIST

#define ERASE_AUTHOR_ON_AUTHLIST(Itr, Var) \
LIST_ERASE_ITEM (AUTHOR_ON_AUTHLIST, Itr, Var)

/// AUTHOR_ON_AUTHLIST_IS_EMPTY

#define AUTHOR_ON_AUTHLIST_IS_EMPTY(Var) \
    FIELD_IS_EMPTY( AUTHOR_ON_AUTHLIST, Var )

///
/// CUser_object macros

/// USERFIELD_ON_USEROBJECT macros

#define USERFIELD_ON_USEROBJECT_Type      CUser_object::TData
#define USERFIELD_ON_USEROBJECT_Test(Var) (Var).IsSetData()
#define USERFIELD_ON_USEROBJECT_Get(Var)  (Var).GetData()
#define USERFIELD_ON_USEROBJECT_Set(Var)  (Var).SetData()

/// USEROBJECT_HAS_USERFIELD

#define USEROBJECT_HAS_USERFIELD(Var) \
ITEM_HAS (USERFIELD_ON_USEROBJECT, Var)

/// FOR_EACH_USERFIELD_ON_USEROBJECT
/// EDIT_EACH_USERFIELD_ON_USEROBJECT
// CUser_object& as input, dereference with [const] CUser_field& fld = **itr;

#define FOR_EACH_USERFIELD_ON_USEROBJECT(Itr, Var) \
FOR_EACH (USERFIELD_ON_USEROBJECT, Itr, Var)

#define EDIT_EACH_USERFIELD_ON_USEROBJECT(Itr, Var) \
EDIT_EACH (USERFIELD_ON_USEROBJECT, Itr, Var)

/// ADD_USERFIELD_TO_USEROBJECT

#define ADD_USERFIELD_TO_USEROBJECT(Var, Ref) \
ADD_ITEM (USERFIELD_ON_USEROBJECT, Var, Ref)

/// ERASE_USERFIELD_ON_USEROBJECT

#define ERASE_USERFIELD_ON_USEROBJECT(Itr, Var) \
VECTOR_ERASE_ITEM (USERFIELD_ON_USEROBJECT, Itr, Var)

#define USERFIELD_ON_USEROBJECT_IS_SORTED(Var, Func) \
IS_SORTED (USERFIELD_ON_USEROBJECT, Var, Func)

#define SORT_USERFIELD_ON_USEROBJECT(Var, Func) \
DO_VECTOR_SORT (USERFIELD_ON_USEROBJECT, Var, Func)

///
/// CUser_field macros

/// USERFIELD_CHOICE macros

#define USERFIELD_CHOICE_Test(Var) (Var).IsSetData() && Var.GetData().Which() != CUser_field::TData::e_not_set
#define USERFIELD_CHOICE_Chs(Var)  (Var).GetData().Which()

/// USERFIELD_CHOICE_IS

#define USERFIELD_CHOICE_IS(Var, Chs) \
CHOICE_IS (USERFIELD_CHOICE, Var, Chs)

/// SWITCH_ON_USERFIELD_CHOICE

#define SWITCH_ON_USERFIELD_CHOICE(Var) \
SWITCH_ON (USERFIELD_CHOICE, Var)


///
/// CGB_block macros

/// EXTRAACCN_ON_GENBANKBLOCK macros

#define EXTRAACCN_ON_GENBANKBLOCK_Type      CGB_block::TExtra_accessions
#define EXTRAACCN_ON_GENBANKBLOCK_Test(Var) (Var).IsSetExtra_accessions()
#define EXTRAACCN_ON_GENBANKBLOCK_Get(Var)  (Var).GetExtra_accessions()
#define EXTRAACCN_ON_GENBANKBLOCK_Set(Var)  (Var).SetExtra_accessions()

/// GENBANKBLOCK_HAS_EXTRAACCN

#define GENBANKBLOCK_HAS_EXTRAACCN(Var) \
ITEM_HAS (EXTRAACCN_ON_GENBANKBLOCK, Var)

/// FOR_EACH_EXTRAACCN_ON_GENBANKBLOCK
/// EDIT_EACH_EXTRAACCN_ON_GENBANKBLOCK
// CGB_block& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_EXTRAACCN_ON_GENBANKBLOCK(Itr, Var) \
FOR_EACH (EXTRAACCN_ON_GENBANKBLOCK, Itr, Var)

#define EDIT_EACH_EXTRAACCN_ON_GENBANKBLOCK(Itr, Var) \
EDIT_EACH (EXTRAACCN_ON_GENBANKBLOCK, Itr, Var)

/// ADD_EXTRAACCN_TO_GENBANKBLOCK

#define ADD_EXTRAACCN_TO_GENBANKBLOCK(Var, Ref) \
ADD_ITEM (EXTRAACCN_ON_GENBANKBLOCK, Var, Ref)

/// ERASE_EXTRAACCN_ON_GENBANKBLOCK

#define ERASE_EXTRAACCN_ON_GENBANKBLOCK(Itr, Var) \
LIST_ERASE_ITEM (EXTRAACCN_ON_GENBANKBLOCK, Itr, Var)

/// EXTRAACCN_ON_GENBANKBLOCK_IS_SORTED

#define EXTRAACCN_ON_GENBANKBLOCK_IS_SORTED(Var, Func) \
IS_SORTED (EXTRAACCN_ON_GENBANKBLOCK, Var, Func)

/// SORT_EXTRAACCN_ON_GENBANKBLOC

#define SORT_EXTRAACCN_ON_GENBANKBLOCK(Var, Func) \
DO_LIST_SORT (EXTRAACCN_ON_GENBANKBLOCK, Var, Func)

/// EXTRAACCN_ON_GENBANKBLOCK_IS_UNIQUE

#define EXTRAACCN_ON_GENBANKBLOCK_IS_UNIQUE(Var, Func) \
IS_UNIQUE (EXTRAACCN_ON_GENBANKBLOCK, Var, Func)

/// UNIQUE_EXTRAACCN_ON_GENBANKBLOCK

#define UNIQUE_EXTRAACCN_ON_GENBANKBLOCK(Var, Func) \
DO_UNIQUE (EXTRAACCN_ON_GENBANKBLOCK, Var, Func)


/// KEYWORD_ON_GENBANKBLOCK macros

#define KEYWORD_ON_GENBANKBLOCK_Type      CGB_block::TKeywords
#define KEYWORD_ON_GENBANKBLOCK_Test(Var) (Var).IsSetKeywords()
#define KEYWORD_ON_GENBANKBLOCK_Get(Var)  (Var).GetKeywords()
#define KEYWORD_ON_GENBANKBLOCK_Set(Var)  (Var).SetKeywords()

/// GENBANKBLOCK_HAS_KEYWORD

#define GENBANKBLOCK_HAS_KEYWORD(Var) \
ITEM_HAS (KEYWORD_ON_GENBANKBLOCK, Var)

/// FOR_EACH_KEYWORD_ON_GENBANKBLOCK
/// EDIT_EACH_KEYWORD_ON_GENBANKBLOCK
// CGB_block& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_KEYWORD_ON_GENBANKBLOCK(Itr, Var) \
FOR_EACH (KEYWORD_ON_GENBANKBLOCK, Itr, Var)

#define EDIT_EACH_KEYWORD_ON_GENBANKBLOCK(Itr, Var) \
EDIT_EACH (KEYWORD_ON_GENBANKBLOCK, Itr, Var)

/// ADD_KEYWORD_TO_GENBANKBLOCK

#define ADD_KEYWORD_TO_GENBANKBLOCK(Var, Ref) \
ADD_ITEM (KEYWORD_ON_GENBANKBLOCK, Var, Ref)

/// ERASE_KEYWORD_ON_GENBANKBLOCK

#define ERASE_KEYWORD_ON_GENBANKBLOCK(Itr, Var) \
LIST_ERASE_ITEM (KEYWORD_ON_GENBANKBLOCK, Itr, Var)

/// KEYWORD_ON_GENBANKBLOCK_IS_SORTED

#define KEYWORD_ON_GENBANKBLOCK_IS_SORTED(Var, Func) \
IS_SORTED (KEYWORD_ON_GENBANKBLOCK, Var, Func)

/// SORT_KEYWORD_ON_GENBANKBLOCK

#define SORT_KEYWORD_ON_GENBANKBLOCK(Var, Func) \
DO_LIST_SORT (KEYWORD_ON_GENBANKBLOCK, Var, Func)

/// KEYWORD_ON_GENBANKBLOCK_IS_UNIQUE

#define KEYWORD_ON_GENBANKBLOCK_IS_UNIQUE(Var, Func) \
IS_UNIQUE (KEYWORD_ON_GENBANKBLOCK, Var, Func)

/// UNIQUE_KEYWORD_ON_GENBANKBLOCK

#define UNIQUE_KEYWORD_ON_GENBANKBLOCK(Var, Func) \
DO_UNIQUE (KEYWORD_ON_GENBANKBLOCK, Var, Func)

/// UNIQUE_WITHOUT_SORT_KEYWORD_ON_GENBANKBLOCK

#define UNIQUE_WITHOUT_SORT_KEYWORD_ON_GENBANKBLOCK(Var, FuncType) \
UNIQUE_WITHOUT_SORT( KEYWORD_ON_GENBANKBLOCK, Var, FuncType, \
    CCleanupChange::eCleanQualifiers )

///
/// CEMBL_block macros

/// EXTRAACCN_ON_EMBLBLOCK macros

#define EXTRAACCN_ON_EMBLBLOCK_Type      CEMBL_block::TExtra_acc
#define EXTRAACCN_ON_EMBLBLOCK_Test(Var) (Var).IsSetExtra_acc()
#define EXTRAACCN_ON_EMBLBLOCK_Get(Var)  (Var).GetExtra_acc()
#define EXTRAACCN_ON_EMBLBLOCK_Set(Var)  (Var).SetExtra_acc()

/// EMBLBLOCK_HAS_EXTRAACCN

#define EMBLBLOCK_HAS_EXTRAACCN(Var) \
ITEM_HAS (EXTRAACCN_ON_EMBLBLOCK, Var)

/// FOR_EACH_EXTRAACCN_ON_EMBLBLOCK
/// EDIT_EACH_EXTRAACCN_ON_EMBLBLOCK
// CEMBL_block& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_EXTRAACCN_ON_EMBLBLOCK(Itr, Var) \
FOR_EACH (EXTRAACCN_ON_EMBLBLOCK, Itr, Var)

#define EDIT_EACH_EXTRAACCN_ON_EMBLBLOCK(Itr, Var) \
EDIT_EACH (EXTRAACCN_ON_EMBLBLOCK, Itr, Var)

/// ADD_EXTRAACCN_TO_EMBLBLOCK

#define ADD_EXTRAACCN_TO_EMBLBLOCK(Var, Ref) \
ADD_ITEM (EXTRAACCN_ON_EMBLBLOCK, Var, Ref)

/// ERASE_EXTRAACCN_ON_EMBLBLOCK

#define ERASE_EXTRAACCN_ON_EMBLBLOCK(Itr, Var) \
LIST_ERASE_ITEM (EXTRAACCN_ON_EMBLBLOCK, Itr, Var)

/// EXTRAACCN_ON_EMBLBLOCK_IS_SORTED

#define EXTRAACCN_ON_EMBLBLOCK_IS_SORTED(Var, Func) \
IS_SORTED (EXTRAACCN_ON_EMBLBLOCK, Var, Func)

/// SORT_EXTRAACCN_ON_EMBLBLOCK

#define SORT_EXTRAACCN_ON_EMBLBLOCK(Var, Func) \
DO_LIST_SORT (EXTRAACCN_ON_EMBLBLOCK, Var, Func)

/// EXTRAACCN_ON_EMBLBLOCK_IS_UNIQUE

#define EXTRAACCN_ON_EMBLBLOCK_IS_UNIQUE(Var, Func) \
IS_UNIQUE (EXTRAACCN_ON_EMBLBLOCK, Var, Func)

/// UNIQUE_EXTRAACCN_ON_EMBLBLOCK

#define UNIQUE_EXTRAACCN_ON_EMBLBLOCK(Var, Func) \
DO_UNIQUE (EXTRAACCN_ON_EMBLBLOCK, Var, Func)


/// KEYWORD_ON_EMBLBLOCK macros

#define KEYWORD_ON_EMBLBLOCK_Type      CEMBL_block::TKeywords
#define KEYWORD_ON_EMBLBLOCK_Test(Var) (Var).IsSetKeywords()
#define KEYWORD_ON_EMBLBLOCK_Get(Var)  (Var).GetKeywords()
#define KEYWORD_ON_EMBLBLOCK_Set(Var)  (Var).SetKeywords()

/// EMBLBLOCK_HAS_KEYWORD

#define EMBLBLOCK_HAS_KEYWORD(Var) \
ITEM_HAS (KEYWORD_ON_EMBLBLOCK, Var)

/// FOR_EACH_KEYWORD_ON_EMBLBLOCK
/// EDIT_EACH_KEYWORD_ON_EMBLBLOCK
// CEMBL_block& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_KEYWORD_ON_EMBLBLOCK(Itr, Var) \
FOR_EACH (KEYWORD_ON_EMBLBLOCK, Itr, Var)

#define EDIT_EACH_KEYWORD_ON_EMBLBLOCK(Itr, Var) \
EDIT_EACH (KEYWORD_ON_EMBLBLOCK, Itr, Var)

/// ADD_KEYWORD_TO_EMBLBLOCK

#define ADD_KEYWORD_TO_EMBLBLOCK(Var, Ref) \
ADD_ITEM (KEYWORD_ON_EMBLBLOCK, Var, Ref)

/// ERASE_KEYWORD_ON_EMBLBLOCK

#define ERASE_KEYWORD_ON_EMBLBLOCK(Itr, Var) \
LIST_ERASE_ITEM (KEYWORD_ON_EMBLBLOCK, Itr, Var)

/// KEYWORD_ON_EMBLBLOCK_IS_SORTED

#define KEYWORD_ON_EMBLBLOCK_IS_SORTED(Var, Func) \
IS_SORTED (KEYWORD_ON_EMBLBLOCK, Var, Func)

/// SORT_KEYWORD_ON_EMBLBLOCK

#define SORT_KEYWORD_ON_EMBLBLOCK(Var, Func) \
DO_LIST_SORT (KEYWORD_ON_EMBLBLOCK, Var, Func)

/// KEYWORD_ON_EMBLBLOCK_IS_UNIQUE

#define KEYWORD_ON_EMBLBLOCK_IS_UNIQUE(Var, Func) \
IS_UNIQUE (KEYWORD_ON_EMBLBLOCK, Var, Func)

/// UNIQUE_KEYWORD_ON_EMBLBLOCK

#define UNIQUE_KEYWORD_ON_EMBLBLOCK(Var, Func) \
DO_UNIQUE (KEYWORD_ON_EMBLBLOCK, Var, Func)


/// UNIQUE_WITHOUT_SORT_KEYWORD_ON_EMBLBLOCK

#define UNIQUE_WITHOUT_SORT_KEYWORD_ON_EMBLBLOCK(Var, FuncType) \
UNIQUE_WITHOUT_SORT(KEYWORD_ON_EMBLBLOCK, Var, FuncType, \
    CCleanupChange::eCleanQualifiers)

///
/// CPDB_block macros

/// COMPOUND_ON_PDBBLOCK macros

#define COMPOUND_ON_PDBBLOCK_Type      CPDB_block::TCompound
#define COMPOUND_ON_PDBBLOCK_Test(Var) (Var).IsSetCompound()
#define COMPOUND_ON_PDBBLOCK_Get(Var)  (Var).GetCompound()
#define COMPOUND_ON_PDBBLOCK_Set(Var)  (Var).SetCompound()

/// PDBBLOCK_HAS_COMPOUND

#define PDBBLOCK_HAS_COMPOUND(Var) \
ITEM_HAS (COMPOUND_ON_PDBBLOCK, Var)

/// FOR_EACH_COMPOUND_ON_PDBBLOCK
/// EDIT_EACH_COMPOUND_ON_PDBBLOCK
// CPDB_block& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_COMPOUND_ON_PDBBLOCK(Itr, Var) \
FOR_EACH (COMPOUND_ON_PDBBLOCK, Itr, Var)

#define EDIT_EACH_COMPOUND_ON_PDBBLOCK(Itr, Var) \
EDIT_EACH (COMPOUND_ON_PDBBLOCK, Itr, Var)

/// ADD_COMPOUND_TO_PDBBLOCK

#define ADD_COMPOUND_TO_PDBBLOCK(Var, Ref) \
ADD_ITEM (COMPOUND_ON_PDBBLOCK, Var, Ref)

/// ERASE_COMPOUND_ON_PDBBLOCK

#define ERASE_COMPOUND_ON_PDBBLOCK(Itr, Var) \
LIST_ERASE_ITEM (COMPOUND_ON_PDBBLOCK, Itr, Var)


/// SOURCE_ON_PDBBLOCK macros

#define SOURCE_ON_PDBBLOCK_Type      CPDB_block::TSource
#define SOURCE_ON_PDBBLOCK_Test(Var) (Var).IsSetSource()
#define SOURCE_ON_PDBBLOCK_Get(Var)  (Var).GetSource()
#define SOURCE_ON_PDBBLOCK_Set(Var)  (Var).SetSource()

/// PDBBLOCK_HAS_SOURCE

#define PDBBLOCK_HAS_SOURCE(Var) \
ITEM_HAS (SOURCE_ON_PDBBLOCK, Var)

/// FOR_EACH_SOURCE_ON_PDBBLOCK
/// EDIT_EACH_SOURCE_ON_PDBBLOCK
// CPDB_block& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_SOURCE_ON_PDBBLOCK(Itr, Var) \
FOR_EACH (SOURCE_ON_PDBBLOCK, Itr, Var)

#define EDIT_EACH_SOURCE_ON_PDBBLOCK(Itr, Var) \
EDIT_EACH (SOURCE_ON_PDBBLOCK, Itr, Var)

/// ADD_SOURCE_TO_PDBBLOCK

#define ADD_SOURCE_TO_PDBBLOCK(Var, Ref) \
ADD_ITEM (SOURCE_ON_PDBBLOCK, Var, Ref)

/// ERASE_SOURCE_ON_PDBBLOCK

#define ERASE_SOURCE_ON_PDBBLOCK(Itr, Var) \
LIST_ERASE_ITEM (SOURCE_ON_PDBBLOCK, Itr, Var)


///
/// CSeq_feat macros

/// SEQFEAT_CHOICE macros

#define SEQFEAT_CHOICE_Test(Var) (Var).IsSetData()
#define SEQFEAT_CHOICE_Chs(Var)  (Var).GetData().Which()

/// SEQFEAT_CHOICE_IS

#define SEQFEAT_CHOICE_IS(Var, Chs) \
CHOICE_IS (SEQFEAT_CHOICE, Var, Chs)

/// SWITCH_ON_SEQFEAT_CHOICE

#define SWITCH_ON_SEQFEAT_CHOICE(Var) \
SWITCH_ON (SEQFEAT_CHOICE, Var)

/// FEATURE_CHOICE_IS
/// SWITCH_ON_FEATURE_CHOICE

#define FEATURE_CHOICE_IS SEQFEAT_CHOICE_IS
#define SWITCH_ON_FEATURE_CHOICE SWITCH_ON_SEQFEAT_CHOICE


/// GBQUAL_ON_SEQFEAT macros

#define GBQUAL_ON_SEQFEAT_Type       CSeq_feat::TQual
#define GBQUAL_ON_SEQFEAT_Test(Var)  (Var).IsSetQual()
#define GBQUAL_ON_SEQFEAT_Get(Var)   (Var).GetQual()
#define GBQUAL_ON_SEQFEAT_Set(Var)   (Var).SetQual()
#define GBQUAL_ON_SEQFEAT_Reset(Var) (Var).ResetQual()

/// SEQFEAT_HAS_GBQUAL

#define SEQFEAT_HAS_GBQUAL(Var) \
ITEM_HAS (GBQUAL_ON_SEQFEAT, Var)

/// FOR_EACH_GBQUAL_ON_SEQFEAT
/// EDIT_EACH_GBQUAL_ON_SEQFEAT
// CSeq_feat& as input, dereference with [const] CGb_qual& gbq = **itr;

#define FOR_EACH_GBQUAL_ON_SEQFEAT(Itr, Var) \
FOR_EACH (GBQUAL_ON_SEQFEAT, Itr, Var)

#define EDIT_EACH_GBQUAL_ON_SEQFEAT(Itr, Var) \
EDIT_EACH (GBQUAL_ON_SEQFEAT, Itr, Var)

/// ADD_GBQUAL_TO_SEQFEAT

#define ADD_GBQUAL_TO_SEQFEAT(Var, Ref) \
ADD_ITEM (GBQUAL_ON_SEQFEAT, Var, Ref)

/// ERASE_GBQUAL_ON_SEQFEAT

#define ERASE_GBQUAL_ON_SEQFEAT(Itr, Var) \
VECTOR_ERASE_ITEM (GBQUAL_ON_SEQFEAT, Itr, Var)

/// GBQUAL_ON_SEQFEAT_IS_SORTED

#define GBQUAL_ON_SEQFEAT_IS_SORTED(Var, Func) \
IS_SORTED (GBQUAL_ON_SEQFEAT, Var, Func)

/// SORT_GBQUAL_ON_SEQFEAT

#define SORT_GBQUAL_ON_SEQFEAT(Var, Func) \
DO_VECTOR_SORT (GBQUAL_ON_SEQFEAT, Var, Func)

/// GBQUAL_ON_SEQFEAT_IS_UNIQUE

#define GBQUAL_ON_SEQFEAT_IS_UNIQUE(Var, Func) \
IS_UNIQUE (GBQUAL_ON_SEQFEAT, Var, Func)

/// UNIQUE_GBQUAL_ON_SEQFEAT

#define UNIQUE_GBQUAL_ON_SEQFEAT(Var, Func) \
DO_UNIQUE (GBQUAL_ON_SEQFEAT, Var, Func)

/// RESET_GBQUAL_ON_SEQFEAT_IF_EMPTY
#define REMOVE_IF_EMPTY_GBQUAL_ON_SEQFEAT(Var) \
    REMOVE_IF_EMPTY_FIELD(GBQUAL_ON_SEQFEAT, Var)

/// FEATURE_HAS_GBQUAL
/// FOR_EACH_GBQUAL_ON_FEATURE
/// EDIT_EACH_GBQUAL_ON_FEATURE
/// ADD_GBQUAL_TO_FEATURE
/// ERASE_GBQUAL_ON_FEATURE
/// GBQUAL_ON_FEATURE_IS_SORTED
/// SORT_GBQUAL_ON_FEATURE
/// GBQUAL_ON_FEATURE_IS_UNIQUE
/// UNIQUE_GBQUAL_ON_FEATURE

#define FEATURE_HAS_GBQUAL SEQFEAT_HAS_GBQUAL
#define FOR_EACH_GBQUAL_ON_FEATURE FOR_EACH_GBQUAL_ON_SEQFEAT
#define EDIT_EACH_GBQUAL_ON_FEATURE EDIT_EACH_GBQUAL_ON_SEQFEAT
#define ADD_GBQUAL_TO_FEATURE ADD_GBQUAL_TO_SEQFEAT
#define ERASE_GBQUAL_ON_FEATURE ERASE_GBQUAL_ON_SEQFEAT
#define GBQUAL_ON_FEATURE_IS_SORTED GBQUAL_ON_SEQFEAT_IS_SORTED
#define SORT_GBQUAL_ON_FEATURE SORT_GBQUAL_ON_SEQFEAT
#define GBQUAL_ON_FEATURE_IS_UNIQUE GBQUAL_ON_SEQFEAT_IS_UNIQUE
#define UNIQUE_GBQUAL_ON_FEATURE UNIQUE_GBQUAL_ON_SEQFEAT


/// SEQFEATXREF_ON_SEQFEAT macros

#define SEQFEATXREF_ON_SEQFEAT_Type       CSeq_feat::TXref
#define SEQFEATXREF_ON_SEQFEAT_Test(Var)  (Var).IsSetXref()
#define SEQFEATXREF_ON_SEQFEAT_Get(Var)   (Var).GetXref()
#define SEQFEATXREF_ON_SEQFEAT_Set(Var)   (Var).SetXref()
#define SEQFEATXREF_ON_SEQFEAT_Reset(Var) (Var).ResetXref()

/// SEQFEAT_HAS_SEQFEATXREF

#define SEQFEAT_HAS_SEQFEATXREF(Var) \
ITEM_HAS (SEQFEATXREF_ON_SEQFEAT, Var)

/// FOR_EACH_SEQFEATXREF_ON_SEQFEAT
/// EDIT_EACH_SEQFEATXREF_ON_SEQFEAT
// CSeq_feat& as input, dereference with [const] CSeqFeatXref& sfx = **itr;

#define FOR_EACH_SEQFEATXREF_ON_SEQFEAT(Itr, Var) \
FOR_EACH (SEQFEATXREF_ON_SEQFEAT, Itr, Var)

#define EDIT_EACH_SEQFEATXREF_ON_SEQFEAT(Itr, Var) \
EDIT_EACH (SEQFEATXREF_ON_SEQFEAT, Itr, Var)

/// ADD_SEQFEATXREF_TO_SEQFEAT

#define ADD_SEQFEATXREF_TO_SEQFEAT(Var, Ref) \
ADD_ITEM (SEQFEATXREF_ON_SEQFEAT, Var, Ref)

/// ERASE_SEQFEATXREF_ON_SEQFEAT

#define ERASE_SEQFEATXREF_ON_SEQFEAT(Itr, Var) \
VECTOR_ERASE_ITEM (SEQFEATXREF_ON_SEQFEAT, Itr, Var)

/// SEQFEATXREF_ON_SEQFEAT_IS_SORTED

#define SEQFEATXREF_ON_SEQFEAT_IS_SORTED(Var, Func) \
IS_SORTED (SEQFEATXREF_ON_SEQFEAT, Var, Func)

/// SORT_SEQFEATXREF_ON_SEQFEAT

#define SORT_SEQFEATXREF_ON_SEQFEAT(Var, Func) \
DO_VECTOR_SORT (SEQFEATXREF_ON_SEQFEAT, Var, Func)

/// SEQFEATXREF_ON_SEQFEAT_IS_UNIQUE

#define SEQFEATXREF_ON_SEQFEAT_IS_UNIQUE(Var, Func) \
IS_UNIQUE (SEQFEATXREF_ON_SEQFEAT, Var, Func)

/// UNIQUE_SEQFEATXREF_ON_SEQFEAT

#define UNIQUE_SEQFEATXREF_ON_SEQFEAT(Var, Func) \
DO_UNIQUE (SEQFEATXREF_ON_SEQFEAT, Var, Func)

/// REMOVE_IF_EMPTY_GBQUAL_ON_SEQFEAT

#define REMOVE_IF_EMPTY_SEQFEATXREF_ON_SEQFEAT(Var) \
    REMOVE_IF_EMPTY_FIELD(SEQFEATXREF_ON_SEQFEAT, Var)

/// FEATURE_HAS_SEQFEATXREF
/// FOR_EACH_SEQFEATXREF_ON_FEATURE
/// EDIT_EACH_SEQFEATXREF_ON_FEATURE
/// ADD_SEQFEATXREF_TO_FEATURE
/// ERASE_SEQFEATXREF_ON_FEATURE
/// SEQFEATXREF_ON_FEATURE_IS_SORTED
/// SORT_SEQFEATXREF_ON_FEATURE
/// SEQFEATXREF_ON_FEATURE_IS_UNIQUE
/// UNIQUE_SEQFEATXREF_ON_FEATURE

#define FEATURE_HAS_SEQFEATXREF SEQFEAT_HAS_SEQFEATXREF
#define FOR_EACH_SEQFEATXREF_ON_FEATURE FOR_EACH_SEQFEATXREF_ON_SEQFEAT
#define EDIT_EACH_SEQFEATXREF_ON_FEATURE EDIT_EACH_SEQFEATXREF_ON_SEQFEAT
#define ADD_SEQFEATXREF_TO_FEATURE ADD_SEQFEATXREF_TO_SEQFEAT
#define ERASE_SEQFEATXREF_ON_FEATURE ERASE_SEQFEATXREF_ON_SEQFEAT
#define SEQFEATXREF_ON_FEATURE_IS_SORTED SEQFEATXREF_ON_SEQFEAT_IS_SORTED
#define SORT_SEQFEATXREF_ON_FEATURE SORT_SEQFEATXREF_ON_SEQFEAT
#define SEQFEATXREF_ON_FEATURE_IS_UNIQUE SEQFEATXREF_ON_SEQFEAT_IS_UNIQUE
#define UNIQUE_SEQFEATXREF_ON_FEATURE UNIQUE_SEQFEATXREF_ON_SEQFEAT

/// XREF_ON_SEQFEAT macros

#define XREF_ON_SEQFEAT_Type       CSeq_feat::TXref
#define XREF_ON_SEQFEAT_Test(Var)  (Var).IsSetXref()
#define XREF_ON_SEQFEAT_Get(Var)   (Var).GetXref()
#define XREF_ON_SEQFEAT_Set(Var)   (Var).SetXref()
#define XREF_ON_SEQFEAT_Reset(Var) (Var).ResetXref()

/// SEQFEAT_HAS_XREF

#define SEQFEAT_HAS_XREF(Var) \
ITEM_HAS (XREF_ON_SEQFEAT, Var)

/// FOR_EACH_XREF_ON_SEQFEAT
/// EDIT_EACH_XREF_ON_SEQFEAT
// CSeq_feat& as input, dereference with [const] CDbtag& dbt = **itr;

#define FOR_EACH_XREF_ON_SEQFEAT(Itr, Var) \
FOR_EACH (XREF_ON_SEQFEAT, Itr, Var)

#define EDIT_EACH_XREF_ON_SEQFEAT(Itr, Var) \
EDIT_EACH (XREF_ON_SEQFEAT, Itr, Var)

/// ADD_XREF_TO_SEQFEAT

#define ADD_XREF_TO_SEQFEAT(Var, Ref) \
ADD_ITEM (XREF_ON_SEQFEAT, Var, Ref)

/// ERASE_XREF_ON_SEQFEAT

#define ERASE_XREF_ON_SEQFEAT(Itr, Var) \
VECTOR_ERASE_ITEM (XREF_ON_SEQFEAT, Itr, Var)

/// XREF_ON_SEQFEAT_IS_SORTED

#define XREF_ON_SEQFEAT_IS_SORTED(Var, Func) \
IS_SORTED (XREF_ON_SEQFEAT, Var, Func)

/// SORT_XREF_ON_SEQFEAT

#define SORT_XREF_ON_SEQFEAT(Var, Func) \
DO_VECTOR_SORT (XREF_ON_SEQFEAT, Var, Func)

/// XREF_ON_SEQFEAT_IS_UNIQUE

#define XREF_ON_SEQFEAT_IS_UNIQUE(Var, Func) \
IS_UNIQUE (XREF_ON_SEQFEAT, Var, Func)

/// UNIQUE_XREF_ON_SEQFEAT

#define UNIQUE_XREF_ON_SEQFEAT(Var, Func) \
DO_UNIQUE (XREF_ON_SEQFEAT, Var, Func)

/// REMOVE_IF_EMPTY_XREF_ON_SEQFEAT

#define REMOVE_IF_EMPTY_XREF_ON_SEQFEAT(Var) \
REMOVE_IF_EMPTY_FIELD(XREF_ON_SEQFEAT, Var)

/// DBXREF_ON_SEQFEAT macros

#define DBXREF_ON_SEQFEAT_Type       CSeq_feat::TDbxref
#define DBXREF_ON_SEQFEAT_Test(Var)  (Var).IsSetDbxref()
#define DBXREF_ON_SEQFEAT_Get(Var)   (Var).GetDbxref()
#define DBXREF_ON_SEQFEAT_Set(Var)   (Var).SetDbxref()
#define DBXREF_ON_SEQFEAT_Reset(Var) (Var).ResetDbxref()

/// SEQFEAT_HAS_DBXREF

#define SEQFEAT_HAS_DBXREF(Var) \
ITEM_HAS (DBXREF_ON_SEQFEAT, Var)

/// FOR_EACH_DBXREF_ON_SEQFEAT
/// EDIT_EACH_DBXREF_ON_SEQFEAT
// CSeq_feat& as input, dereference with [const] CDbtag& dbt = **itr;

#define FOR_EACH_DBXREF_ON_SEQFEAT(Itr, Var) \
FOR_EACH (DBXREF_ON_SEQFEAT, Itr, Var)

#define EDIT_EACH_DBXREF_ON_SEQFEAT(Itr, Var) \
EDIT_EACH (DBXREF_ON_SEQFEAT, Itr, Var)

/// ADD_DBXREF_TO_SEQFEAT

#define ADD_DBXREF_TO_SEQFEAT(Var, Ref) \
ADD_ITEM (DBXREF_ON_SEQFEAT, Var, Ref)

/// ERASE_DBXREF_ON_SEQFEAT

#define ERASE_DBXREF_ON_SEQFEAT(Itr, Var) \
VECTOR_ERASE_ITEM (DBXREF_ON_SEQFEAT, Itr, Var)

/// DBXREF_ON_SEQFEAT_IS_SORTED

#define DBXREF_ON_SEQFEAT_IS_SORTED(Var, Func) \
IS_SORTED (DBXREF_ON_SEQFEAT, Var, Func)

/// SORT_DBXREF_ON_SEQFEAT

#define SORT_DBXREF_ON_SEQFEAT(Var, Func) \
DO_VECTOR_SORT (DBXREF_ON_SEQFEAT, Var, Func)

/// DBXREF_ON_SEQFEAT_IS_UNIQUE

#define DBXREF_ON_SEQFEAT_IS_UNIQUE(Var, Func) \
IS_UNIQUE (DBXREF_ON_SEQFEAT, Var, Func)

/// UNIQUE_DBXREF_ON_SEQFEAT

#define UNIQUE_DBXREF_ON_SEQFEAT(Var, Func) \
DO_UNIQUE (DBXREF_ON_SEQFEAT, Var, Func)

/// REMOVE_IF_EMPTY_DBXREF_ON_SEQFEAT

#define REMOVE_IF_EMPTY_DBXREF_ON_SEQFEAT(Var) \
REMOVE_IF_EMPTY_FIELD(DBXREF_ON_SEQFEAT, Var)

/// FEATURE_HAS_DBXREF
/// FOR_EACH_DBXREF_ON_FEATURE
/// EDIT_EACH_DBXREF_ON_FEATURE
/// ADD_DBXREF_TO_FEATURE
/// ERASE_DBXREF_ON_FEATURE
/// DBXREF_ON_FEATURE_IS_SORTED
/// SORT_DBXREF_ON_FEATURE
/// DBXREF_ON_FEATURE_IS_UNIQUE
/// UNIQUE_DBXREF_ON_FEATURE

#define FEATURE_HAS_DBXREF SEQFEAT_HAS_DBXREF
#define FOR_EACH_DBXREF_ON_FEATURE FOR_EACH_DBXREF_ON_SEQFEAT
#define EDIT_EACH_DBXREF_ON_FEATURE EDIT_EACH_DBXREF_ON_SEQFEAT
#define ADD_DBXREF_TO_FEATURE ADD_DBXREF_TO_SEQFEAT
#define ERASE_DBXREF_ON_FEATURE ERASE_DBXREF_ON_SEQFEAT
#define DBXREF_ON_FEATURE_IS_SORTED DBXREF_ON_SEQFEAT_IS_SORTED
#define SORT_DBXREF_ON_FEATURE SORT_DBXREF_ON_SEQFEAT
#define DBXREF_ON_FEATURE_IS_UNIQUE DBXREF_ON_SEQFEAT_IS_UNIQUE
#define UNIQUE_DBXREF_ON_FEATURE UNIQUE_DBXREF_ON_SEQFEAT


///
/// CSeqFeatData macros

/// SEQFEATDATA_CHOICE macros

#define SEQFEATDATA_CHOICE_Test(Var) (Var).Which() != CSeqFeatData::e_not_set
#define SEQFEATDATA_CHOICE_Chs(Var)  (Var).Which()

/// SEQFEATDATA_CHOICE_IS

#define SEQFEATDATA_CHOICE_IS(Var, Chs) \
CHOICE_IS (SEQFEATDATA_CHOICE, Var, Chs)

/// SWITCH_ON_SEQFEATDATA_CHOICE

#define SWITCH_ON_SEQFEATDATA_CHOICE(Var) \
SWITCH_ON (SEQFEATDATA_CHOICE, Var)


///
/// CSeqFeatXref macros

/// SEQFEATXREF_CHOICE macros

#define SEQFEATXREF_CHOICE_Test(Var) (Var).IsSetData()
#define SEQFEATXREF_CHOICE_Chs(Var)  (Var).GetData().Which()

/// SEQFEATXREF_CHOICE_IS

#define SEQFEATXREF_CHOICE_IS(Var, Chs) \
CHOICE_IS (SEQFEATXREF_CHOICE, Var, Chs)

/// SWITCH_ON_SEQFEATXREF_CHOICE

#define SWITCH_ON_SEQFEATXREF_CHOICE(Var) \
SWITCH_ON (SEQFEATXREF_CHOICE, Var)


///
/// CGene_ref macros

/// SYNONYM_ON_GENEREF macros

#define SYNONYM_ON_GENEREF_Type      CGene_ref::TSyn
#define SYNONYM_ON_GENEREF_Test(Var) (Var).IsSetSyn()
#define SYNONYM_ON_GENEREF_Get(Var)  (Var).GetSyn()
#define SYNONYM_ON_GENEREF_Set(Var)  (Var).SetSyn()

/// GENEREF_HAS_SYNONYM

#define GENEREF_HAS_SYNONYM(Var) \
ITEM_HAS (SYNONYM_ON_GENEREF, Var)

/// FOR_EACH_SYNONYM_ON_GENEREF
/// EDIT_EACH_SYNONYM_ON_GENEREF
// CGene_ref& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_SYNONYM_ON_GENEREF(Itr, Var) \
FOR_EACH (SYNONYM_ON_GENEREF, Itr, Var)

#define EDIT_EACH_SYNONYM_ON_GENEREF(Itr, Var) \
EDIT_EACH (SYNONYM_ON_GENEREF, Itr, Var)

/// ADD_SYNONYM_TO_GENEREF

#define ADD_SYNONYM_TO_GENEREF(Var, Ref) \
ADD_ITEM (SYNONYM_ON_GENEREF, Var, Ref)

/// ERASE_SYNONYM_ON_GENEREF

#define ERASE_SYNONYM_ON_GENEREF(Itr, Var) \
LIST_ERASE_ITEM (SYNONYM_ON_GENEREF, Itr, Var)

/// SYNONYM_ON_GENEREF_IS_SORTED

#define SYNONYM_ON_GENEREF_IS_SORTED(Var, Func) \
IS_SORTED (SYNONYM_ON_GENEREF, Var, Func)

/// SORT_SYNONYM_ON_GENEREF

#define SORT_SYNONYM_ON_GENEREF(Var, Func) \
DO_LIST_SORT (SYNONYM_ON_GENEREF, Var, Func)

/// SYNONYM_ON_GENEREF_IS_UNIQUE

#define SYNONYM_ON_GENEREF_IS_UNIQUE(Var, Func) \
IS_UNIQUE (SYNONYM_ON_GENEREF, Var, Func)

/// UNIQUE_SYNONYM_ON_GENEREF

#define UNIQUE_SYNONYM_ON_GENEREF(Var, Func) \
DO_UNIQUE (SYNONYM_ON_GENEREF, Var, Func)

/// GENE_HAS_SYNONYM
/// FOR_EACH_SYNONYM_ON_GENE
/// EDIT_EACH_SYNONYM_ON_GENE
/// ADD_SYNONYM_TO_GENE
/// ERASE_SYNONYM_ON_GENE
/// SYNONYM_ON_GENE_IS_SORTED
/// SORT_SYNONYM_ON_GENE
/// SYNONYM_ON_GENE_IS_UNIQUE
/// UNIQUE_SYNONYM_ON_GENE

#define GENE_HAS_SYNONYM GENEREF_HAS_SYNONYM
#define FOR_EACH_SYNONYM_ON_GENE FOR_EACH_SYNONYM_ON_GENEREF
#define EDIT_EACH_SYNONYM_ON_GENE EDIT_EACH_SYNONYM_ON_GENEREF
#define ADD_SYNONYM_TO_GENE ADD_SYNONYM_TO_GENEREF
#define ERASE_SYNONYM_ON_GENE ERASE_SYNONYM_ON_GENEREF
#define SYNONYM_ON_GENE_IS_SORTED SYNONYM_ON_GENEREF_IS_SORTED
#define SORT_SYNONYM_ON_GENE SORT_SYNONYM_ON_GENEREF
#define SYNONYM_ON_GENE_IS_UNIQUE SYNONYM_ON_GENEREF_IS_UNIQUE
#define UNIQUE_SYNONYM_ON_GENE UNIQUE_SYNONYM_ON_GENEREF


/// DBXREF_ON_GENEREF macros

#define DBXREF_ON_GENEREF_Type      CGene_ref::TDb
#define DBXREF_ON_GENEREF_Test(Var) (Var).IsSetDb()
#define DBXREF_ON_GENEREF_Get(Var)  (Var).GetDb()
#define DBXREF_ON_GENEREF_Set(Var)  (Var).SetDb()

/// GENEREF_HAS_DBXREF

#define GENEREF_HAS_DBXREF(Var) \
ITEM_HAS (DBXREF_ON_GENEREF, Var)

/// FOR_EACH_DBXREF_ON_GENEREF
/// EDIT_EACH_DBXREF_ON_GENEREF
// CGene_ref& as input, dereference with [const] CDbtag& dbt = **itr;

#define FOR_EACH_DBXREF_ON_GENEREF(Itr, Var) \
FOR_EACH (DBXREF_ON_GENEREF, Itr, Var)

#define EDIT_EACH_DBXREF_ON_GENEREF(Itr, Var) \
EDIT_EACH (DBXREF_ON_GENEREF, Itr, Var)

/// ADD_DBXREF_TO_GENEREF

#define ADD_DBXREF_TO_GENEREF(Var, Ref) \
ADD_ITEM (DBXREF_ON_GENEREF, Var, Ref)

/// ERASE_DBXREF_ON_GENEREF

#define ERASE_DBXREF_ON_GENEREF(Itr, Var) \
VECTOR_ERASE_ITEM (DBXREF_ON_GENEREF, Itr, Var)

/// DBXREF_ON_GENEREF_IS_SORTED

#define DBXREF_ON_GENEREF_IS_SORTED(Var, Func) \
IS_SORTED (DBXREF_ON_GENEREF, Var, Func)

/// SORT_DBXREF_ON_GENEREF

#define SORT_DBXREF_ON_GENEREF(Var, Func) \
DO_VECTOR_SORT (DBXREF_ON_GENEREF, Var, Func)

/// DBXREF_ON_GENEREF_IS_UNIQUE

#define DBXREF_ON_GENEREF_IS_UNIQUE(Var, Func) \
IS_UNIQUE (DBXREF_ON_GENEREF, Var, Func)

/// UNIQUE_DBXREF_ON_GENEREF

#define UNIQUE_DBXREF_ON_GENEREF(Var, Func) \
DO_UNIQUE (DBXREF_ON_GENEREF, Var, Func)

/// GENE_HAS_DBXREF
/// FOR_EACH_DBXREF_ON_GENE
/// EDIT_EACH_DBXREF_ON_GENE
/// ADD_DBXREF_TO_GENE
/// ERASE_DBXREF_ON_GENE
/// DBXREF_ON_GENE_IS_SORTED
/// SORT_DBXREF_ON_GENE
/// DBXREF_ON_GENE_IS_UNIQUE
/// UNIQUE_DBXREF_ON_GENE

#define GENE_HAS_DBXREF GENEREF_HAS_DBXREF
#define FOR_EACH_DBXREF_ON_GENE FOR_EACH_DBXREF_ON_GENEREF
#define EDIT_EACH_DBXREF_ON_GENE EDIT_EACH_DBXREF_ON_GENEREF
#define ADD_DBXREF_TO_GENE ADD_DBXREF_TO_GENEREF
#define ERASE_DBXREF_ON_GENE ERASE_DBXREF_ON_GENEREF
#define DBXREF_ON_GENE_IS_SORTED DBXREF_ON_GENEREF_IS_SORTED
#define SORT_DBXREF_ON_GENE SORT_DBXREF_ON_GENEREF
#define DBXREF_ON_GENE_IS_UNIQUE DBXREF_ON_GENEREF_IS_UNIQUE
#define UNIQUE_DBXREF_ON_GENE UNIQUE_DBXREF_ON_GENEREF


///
/// CCdregion macros

/// CODEBREAK_ON_CDREGION macros

#define CODEBREAK_ON_CDREGION_Type      CCdregion::TCode_break
#define CODEBREAK_ON_CDREGION_Test(Var) (Var).IsSetCode_break()
#define CODEBREAK_ON_CDREGION_Get(Var)  (Var).GetCode_break()
#define CODEBREAK_ON_CDREGION_Set(Var)  (Var).SetCode_break()

/// CDREGION_HAS_CODEBREAK

#define CDREGION_HAS_CODEBREAK(Var) \
ITEM_HAS (CODEBREAK_ON_CDREGION, Var)

/// FOR_EACH_CODEBREAK_ON_CDREGION
/// EDIT_EACH_CODEBREAK_ON_CDREGION
// CCdregion& as input, dereference with [const] CCode_break& cbk = **itr;

#define FOR_EACH_CODEBREAK_ON_CDREGION(Itr, Var) \
FOR_EACH (CODEBREAK_ON_CDREGION, Itr, Var)

#define EDIT_EACH_CODEBREAK_ON_CDREGION(Itr, Var) \
EDIT_EACH (CODEBREAK_ON_CDREGION, Itr, Var)

/// ADD_CODEBREAK_TO_CDREGION

#define ADD_CODEBREAK_TO_CDREGION(Var, Ref) \
ADD_ITEM (CODEBREAK_ON_CDREGION, Var, Ref)

/// ERASE_CODEBREAK_ON_CDREGION

#define ERASE_CODEBREAK_ON_CDREGION(Itr, Var) \
LIST_ERASE_ITEM (CODEBREAK_ON_CDREGION, Itr, Var)

/// CODEBREAK_ON_CDREGION_IS_SORTED

#define CODEBREAK_ON_CDREGION_IS_SORTED(Var, Func) \
IS_SORTED (CODEBREAK_ON_CDREGION, Var, Func)

/// SORT_CODEBREAK_ON_CDREGION

#define SORT_CODEBREAK_ON_CDREGION(Var, Func) \
DO_LIST_SORT_HACK(CODEBREAK_ON_CDREGION, Var, Func)

/// CODEBREAK_ON_CDREGION_IS_UNIQUE

#define CODEBREAK_ON_CDREGION_IS_UNIQUE(Var, Func) \
IS_UNIQUE (CODEBREAK_ON_CDREGION, Var, Func)

/// UNIQUE_CODEBREAK_ON_CDREGION

#define UNIQUE_CODEBREAK_ON_CDREGION(Var, Func) \
DO_UNIQUE (CODEBREAK_ON_CDREGION, Var, Func)


///
/// CProt_ref macros

/// NAME_ON_PROTREF macros

#define NAME_ON_PROTREF_Type       CProt_ref::TName
#define NAME_ON_PROTREF_Test(Var)  (Var).IsSetName()
#define NAME_ON_PROTREF_Get(Var)   (Var).GetName()
#define NAME_ON_PROTREF_Set(Var)   (Var).SetName()
#define NAME_ON_PROTREF_Reset(Var) (Var).ResetName()

/// PROTREF_HAS_NAME

#define PROTREF_HAS_NAME(Var) \
ITEM_HAS (NAME_ON_PROTREF, Var)

/// FOR_EACH_NAME_ON_PROTREF
/// EDIT_EACH_NAME_ON_PROTREF
// CProt_ref& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_NAME_ON_PROTREF(Itr, Var) \
FOR_EACH (NAME_ON_PROTREF, Itr, Var)

#define EDIT_EACH_NAME_ON_PROTREF(Itr, Var) \
EDIT_EACH (NAME_ON_PROTREF, Itr, Var)

/// ADD_NAME_TO_PROTREF

#define ADD_NAME_TO_PROTREF(Var, Ref) \
ADD_ITEM (NAME_ON_PROTREF, Var, Ref)

/// ERASE_NAME_ON_PROTREF

#define ERASE_NAME_ON_PROTREF(Itr, Var) \
LIST_ERASE_ITEM (NAME_ON_PROTREF, Itr, Var)

/// NAME_ON_PROTREF_IS_SORTED

#define NAME_ON_PROTREF_IS_SORTED(Var, Func) \
IS_SORTED (NAME_ON_PROTREF, Var, Func)

/// SORT_NAME_ON_PROTREF

#define SORT_NAME_ON_PROTREF(Var, Func) \
DO_LIST_SORT (NAME_ON_PROTREF, Var, Func)

/// NAME_ON_PROTREF_IS_UNIQUE

#define NAME_ON_PROTREF_IS_UNIQUE(Var, Func) \
IS_UNIQUE (NAME_ON_PROTREF, Var, Func)

/// UNIQUE_NAME_ON_PROTREF

#define UNIQUE_NAME_ON_PROTREF(Var, Func) \
DO_UNIQUE (NAME_ON_PROTREF, Var, Func)

#define REMOVE_IF_EMPTY_NAME_ON_PROTREF(Var) \
    REMOVE_IF_EMPTY_FIELD(NAME_ON_PROTREF, Var)

#define NAME_ON_PROTREF_IS_EMPTY(Var) \
    FIELD_IS_EMPTY( NAME_ON_PROTREF, Var )    

/// PROT_HAS_NAME
/// FOR_EACH_NAME_ON_PROT
/// EDIT_EACH_NAME_ON_PROT
/// ADD_NAME_TO_PROT
/// ERASE_NAME_ON_PROT
/// NAME_ON_PROT_IS_SORTED
/// SORT_NAME_ON_PROT
/// NAME_ON_PROT_IS_UNIQUE
/// UNIQUE_NAME_ON_PROT

#define PROT_HAS_NAME PROTREF_HAS_NAME
#define FOR_EACH_NAME_ON_PROT FOR_EACH_NAME_ON_PROTREF
#define EDIT_EACH_NAME_ON_PROT EDIT_EACH_NAME_ON_PROTREF
#define ADD_NAME_TO_PROT ADD_NAME_TO_PROTREF
#define ERASE_NAME_ON_PROT ERASE_NAME_ON_PROTREF
#define NAME_ON_PROT_IS_SORTED NAME_ON_PROTREF_IS_SORTED
#define SORT_NAME_ON_PROT SORT_NAME_ON_PROTREF
#define NAME_ON_PROT_IS_UNIQUE NAME_ON_PROTREF_IS_UNIQUE
#define UNIQUE_NAME_ON_PROT UNIQUE_NAME_ON_PROTREF


/// ECNUMBER_ON_PROTREF macros

#define ECNUMBER_ON_PROTREF_Type      CProt_ref::TEc
#define ECNUMBER_ON_PROTREF_Test(Var) (Var).IsSetEc()
#define ECNUMBER_ON_PROTREF_Get(Var)  (Var).GetEc()
#define ECNUMBER_ON_PROTREF_Set(Var)  (Var).SetEc()

/// PROTREF_HAS_ECNUMBER

#define PROTREF_HAS_ECNUMBER(Var) \
ITEM_HAS (ECNUMBER_ON_PROTREF, Var)

/// FOR_EACH_ECNUMBER_ON_PROTREF
/// EDIT_EACH_ECNUMBER_ON_PROTREF
// CProt_ref& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_ECNUMBER_ON_PROTREF(Itr, Var) \
FOR_EACH (ECNUMBER_ON_PROTREF, Itr, Var)

#define EDIT_EACH_ECNUMBER_ON_PROTREF(Itr, Var) \
EDIT_EACH (ECNUMBER_ON_PROTREF, Itr, Var)

/// ADD_ECNUMBER_TO_PROTREF

#define ADD_ECNUMBER_TO_PROTREF(Var, Ref) \
ADD_ITEM (ECNUMBER_ON_PROTREF, Var, Ref)

/// ERASE_ECNUMBER_ON_PROTREF

#define ERASE_ECNUMBER_ON_PROTREF(Itr, Var) \
LIST_ERASE_ITEM (ECNUMBER_ON_PROTREF, Itr, Var)

/// ECNUMBER_ON_PROTREF_IS_SORTED

#define ECNUMBER_ON_PROTREF_IS_SORTED(Var, Func) \
IS_SORTED (ECNUMBER_ON_PROTREF, Var, Func)

/// SORT_ECNUMBER_ON_PROTREF

#define SORT_ECNUMBER_ON_PROTREF(Var, Func) \
DO_LIST_SORT (ECNUMBER_ON_PROTREF, Var, Func)

/// ECNUMBER_ON_PROTREF_IS_UNIQUE

#define ECNUMBER_ON_PROTREF_IS_UNIQUE(Var, Func) \
IS_UNIQUE (ECNUMBER_ON_PROTREF, Var, Func)

/// UNIQUE_ECNUMBER_ON_PROTREF

#define UNIQUE_ECNUMBER_ON_PROTREF(Var, Func) \
DO_UNIQUE (ECNUMBER_ON_PROTREF, Var, Func)

/// PROT_HAS_ECNUMBER
/// FOR_EACH_ECNUMBER_ON_PROT
/// EDIT_EACH_ECNUMBER_ON_PROT
/// ADD_ECNUMBER_TO_PROT
/// ERASE_ECNUMBER_ON_PROT
/// ECNUMBER_ON_PROT_IS_SORTED
/// SORT_ECNUMBER_ON_PROT
/// ECNUMBER_ON_PROT_IS_UNIQUE
/// UNIQUE_ECNUMBER_ON_PROT

#define PROT_HAS_ECNUMBER PROTREF_HAS_ECNUMBER
#define FOR_EACH_ECNUMBER_ON_PROT FOR_EACH_ECNUMBER_ON_PROTREF
#define EDIT_EACH_ECNUMBER_ON_PROT EDIT_EACH_ECNUMBER_ON_PROTREF
#define ADD_ECNUMBER_TO_PROT ADD_ECNUMBER_TO_PROTREF
#define ERASE_ECNUMBER_ON_PROT ERASE_ECNUMBER_ON_PROTREF
#define ECNUMBER_ON_PROT_IS_SORTED ECNUMBER_ON_PROTREF_IS_SORTED
#define SORT_ECNUMBER_ON_PROT SORT_ECNUMBER_ON_PROTREF
#define ECNUMBER_ON_PROT_IS_UNIQUE ECNUMBER_ON_PROTREF_IS_UNIQUE
#define UNIQUE_ECNUMBER_ON_PROT UNIQUE_ECNUMBER_ON_PROTREF


/// ACTIVITY_ON_PROTREF macros

#define ACTIVITY_ON_PROTREF_Type       CProt_ref::TActivity
#define ACTIVITY_ON_PROTREF_Test(Var)  (Var).IsSetActivity()
#define ACTIVITY_ON_PROTREF_Get(Var)   (Var).GetActivity()
#define ACTIVITY_ON_PROTREF_Set(Var)   (Var).SetActivity()
#define ACTIVITY_ON_PROTREF_Reset(Var) (Var).ResetActivity()

/// PROTREF_HAS_ACTIVITY

#define PROTREF_HAS_ACTIVITY(Var) \
ITEM_HAS (ACTIVITY_ON_PROTREF, Var)

/// FOR_EACH_ACTIVITY_ON_PROTREF
/// EDIT_EACH_ACTIVITY_ON_PROTREF
// CProt_ref& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_ACTIVITY_ON_PROTREF(Itr, Var) \
FOR_EACH (ACTIVITY_ON_PROTREF, Itr, Var)

#define EDIT_EACH_ACTIVITY_ON_PROTREF(Itr, Var) \
EDIT_EACH (ACTIVITY_ON_PROTREF, Itr, Var)

/// ADD_ACTIVITY_TO_PROTREF

#define ADD_ACTIVITY_TO_PROTREF(Var, Ref) \
ADD_ITEM (ACTIVITY_ON_PROTREF, Var, Ref)

/// ERASE_ACTIVITY_ON_PROTREF

#define ERASE_ACTIVITY_ON_PROTREF(Itr, Var) \
LIST_ERASE_ITEM (ACTIVITY_ON_PROTREF, Itr, Var)

/// ACTIVITY_ON_PROTREF_IS_SORTED

#define ACTIVITY_ON_PROTREF_IS_SORTED(Var, Func) \
IS_SORTED (ACTIVITY_ON_PROTREF, Var, Func)

/// SORT_ACTIVITY_ON_PROTREF

#define SORT_ACTIVITY_ON_PROTREF(Var, Func) \
DO_LIST_SORT (ACTIVITY_ON_PROTREF, Var, Func)

/// ACTIVITY_ON_PROTREF_IS_UNIQUE

#define ACTIVITY_ON_PROTREF_IS_UNIQUE(Var, Func) \
IS_UNIQUE (ACTIVITY_ON_PROTREF, Var, Func)

/// UNIQUE_ACTIVITY_ON_PROTREF

#define UNIQUE_ACTIVITY_ON_PROTREF(Var, Func) \
DO_UNIQUE (ACTIVITY_ON_PROTREF, Var, Func)

/// UNIQUE_WITHOUT_SORT_ACTIVITY_ON_PROTREF(Var, Func)

#define UNIQUE_WITHOUT_SORT_ACTIVITY_ON_PROTREF(Var, FuncType ) \
UNIQUE_WITHOUT_SORT( ACTIVITY_ON_PROTREF, Var, FuncType, \
    CCleanupChange::eChangeProtActivities)

/// REMOVE_IF_EMPTY_ACTIVITY_ON_PROTREF

#define REMOVE_IF_EMPTY_ACTIVITY_ON_PROTREF(Var) \
    REMOVE_IF_EMPTY_FIELD( ACTIVITY_ON_PROTREF, Var )

/// PROT_HAS_ACTIVITY
/// FOR_EACH_ACTIVITY_ON_PROT
/// EDIT_EACH_ACTIVITY_ON_PROT
/// ADD_ACTIVITY_TO_PROT
/// ERASE_ACTIVITY_ON_PROT
/// ACTIVITY_ON_PROT_IS_SORTED
/// SORT_ACTIVITY_ON_PROT
/// ACTIVITY_ON_PROT_IS_UNIQUE
/// UNIQUE_ACTIVITY_ON_PROT

#define PROT_HAS_ACTIVITY PROTREF_HAS_ACTIVITY
#define FOR_EACH_ACTIVITY_ON_PROT FOR_EACH_ACTIVITY_ON_PROTREF
#define EDIT_EACH_ACTIVITY_ON_PROT EDIT_EACH_ACTIVITY_ON_PROTREF
#define ADD_ACTIVITY_TO_PROT ADD_ACTIVITY_TO_PROTREF
#define ERASE_ACTIVITY_ON_PROT ERASE_ACTIVITY_ON_PROTREF
#define ACTIVITY_ON_PROT_IS_SORTED ACTIVITY_ON_PROTREF_IS_SORTED
#define SORT_ACTIVITY_ON_PROT SORT_ACTIVITY_ON_PROTREF
#define ACTIVITY_ON_PROT_IS_UNIQUE ACTIVITY_ON_PROTREF_IS_UNIQUE
#define UNIQUE_ACTIVITY_ON_PROT UNIQUE_ACTIVITY_ON_PROTREF


/// DBXREF_ON_PROTREF macros

#define DBXREF_ON_PROTREF_Type      CProt_ref::TDb
#define DBXREF_ON_PROTREF_Test(Var) (Var).IsSetDb()
#define DBXREF_ON_PROTREF_Get(Var)  (Var).GetDb()
#define DBXREF_ON_PROTREF_Set(Var)  (Var).SetDb()

/// PROTREF_HAS_DBXREF

#define PROTREF_HAS_DBXREF(Var) \
ITEM_HAS (DBXREF_ON_PROTREF, Var)

/// FOR_EACH_DBXREF_ON_PROTREF
/// EDIT_EACH_DBXREF_ON_PROTREF
// CProt_ref& as input, dereference with [const] CDbtag& dbt = *itr;

#define FOR_EACH_DBXREF_ON_PROTREF(Itr, Var) \
FOR_EACH (DBXREF_ON_PROTREF, Itr, Var)

#define EDIT_EACH_DBXREF_ON_PROTREF(Itr, Var) \
EDIT_EACH (DBXREF_ON_PROTREF, Itr, Var)

/// ADD_DBXREF_TO_PROTREF

#define ADD_DBXREF_TO_PROTREF(Var, Ref) \
ADD_ITEM (DBXREF_ON_PROTREF, Var, Ref)

/// ERASE_DBXREF_ON_PROTREF

#define ERASE_DBXREF_ON_PROTREF(Itr, Var) \
VECTOR_ERASE_ITEM (DBXREF_ON_PROTREF, Itr, Var)

/// DBXREF_ON_PROTREF_IS_SORTED

#define DBXREF_ON_PROTREF_IS_SORTED(Var, Func) \
IS_SORTED (DBXREF_ON_PROTREF, Var, Func)

/// SORT_DBXREF_ON_PROTREF

#define SORT_DBXREF_ON_PROTREF(Var, Func) \
DO_VECTOR_SORT (DBXREF_ON_PROTREF, Var, Func)

/// DBXREF_ON_PROTREF_IS_UNIQUE

#define DBXREF_ON_PROTREF_IS_UNIQUE(Var, Func) \
IS_UNIQUE (DBXREF_ON_PROTREF, Var, Func)

/// UNIQUE_DBXREF_ON_PROTREF

#define UNIQUE_DBXREF_ON_PROTREF(Var, Func) \
DO_UNIQUE (DBXREF_ON_PROTREF, Var, Func)

/// PROT_HAS_DBXREF
/// FOR_EACH_DBXREF_ON_PROT
/// EDIT_EACH_DBXREF_ON_PROT
/// ADD_DBXREF_TO_PROT
/// ERASE_DBXREF_ON_PROT
/// DBXREF_ON_PROT_IS_SORTED
/// SORT_DBXREF_ON_PROT
/// DBXREF_ON_PROT_IS_UNIQUE
/// UNIQUE_DBXREF_ON_PROT

#define PROT_HAS_DBXREF PROTREF_HAS_DBXREF
#define FOR_EACH_DBXREF_ON_PROT FOR_EACH_DBXREF_ON_PROTREF
#define EDIT_EACH_DBXREF_ON_PROT EDIT_EACH_DBXREF_ON_PROTREF
#define ADD_DBXREF_TO_PROT ADD_DBXREF_TO_PROTREF
#define ERASE_DBXREF_ON_PROT ERASE_DBXREF_ON_PROTREF
#define DBXREF_ON_PROT_IS_SORTED DBXREF_ON_PROTREF_IS_SORTED
#define SORT_DBXREF_ON_PROT SORT_DBXREF_ON_PROTREF
#define DBXREF_ON_PROT_IS_UNIQUE DBXREF_ON_PROTREF_IS_UNIQUE
#define UNIQUE_DBXREF_ON_PROT UNIQUE_DBXREF_ON_PROTREF


///
/// CRNA_gen macros

/// QUAL_ON_RNAGEN macros

#define QUAL_ON_RNAGEN_Type      CRNA_gen::TQuals::Tdata
#define QUAL_ON_RNAGEN_Test(Var) (Var).IsSetQuals() && (Var).GetQuals().IsSet()
#define QUAL_ON_RNAGEN_Get(Var)  (Var).GetQuals().Get()
#define QUAL_ON_RNAGEN_Set(Var)  (Var).SetQuals().Set()

/// RNAGEN_HAS_QUAL

#define RNAGEN_HAS_QUAL(Var) \
ITEM_HAS (QUAL_ON_RNAGEN, Var)

/// FOR_EACH_QUAL_ON_RNAGEN
/// EDIT_EACH_QUAL_ON_RNAGEN
// CRNA_gen& as input, dereference with [const] CRNA_qual& qual = **itr;

#define FOR_EACH_QUAL_ON_RNAGEN(Itr, Var) \
FOR_EACH (QUAL_ON_RNAGEN, Itr, Var)

#define EDIT_EACH_QUAL_ON_RNAGEN(Itr, Var) \
EDIT_EACH (QUAL_ON_RNAGEN, Itr, Var)

/// ADD_QUAL_TO_RNAGEN

#define ADD_QUAL_TO_RNAGEN(Var, Ref) \
ADD_ITEM (QUAL_ON_RNAGEN, Var, Ref)

/// ERASE_QUAL_ON_RNAGEN

#define ERASE_QUAL_ON_RNAGEN(Itr, Var) \
LIST_ERASE_ITEM (QUAL_ON_RNAGEN, Itr, Var)

/// QUAL_ON_RNAGEN_IS_SORTED

#define QUAL_ON_RNAGEN_IS_SORTED(Var, Func) \
IS_SORTED (QUAL_ON_RNAGEN, Var, Func)

/// SORT_QUAL_ON_RNAGEN

#define SORT_QUAL_ON_RNAGEN(Var, Func) \
DO_LIST_SORT (QUAL_ON_RNAGEN, Var, Func)
 
/// QUAL_ON_RNAGEN_IS_UNIQUE

#define QUAL_ON_RNAGEN_IS_UNIQUE(Var, Func) \
IS_UNIQUE (QUAL_ON_RNAGEN, Var, Func)

/// UNIQUE_QUAL_ON_RNAGEN

#define UNIQUE_QUAL_ON_RNAGEN(Var, Func) \
DO_UNIQUE (QUAL_ON_RNAGEN, Var, Func)

/// REMOVE_IF_EMPTY_QUAL_ON_RNAGEN

#define REMOVE_IF_EMPTY_QUAL_ON_RNAGEN(Var) \
    REMOVE_IF_EMPTY_FIELD(QUAL_ON_RNAGEN, Var)

/// QUAL_ON_RNAGEN_IS_EMPTY

#define QUAL_ON_RNAGEN_IS_EMPTY(Var) \
    FIELD_IS_EMPTY(QUAL_ON_RNAGEN, Var, Func)


///
/// CRNA_qual_set macros

/// QUAL_ON_RNAQSET macros

#define QUAL_ON_RNAQSET_Type       CRNA_qual_set::Tdata
#define QUAL_ON_RNAQSET_Test(Var)  (Var).IsSet()
#define QUAL_ON_RNAQSET_Get(Var)   (Var).Get()
#define QUAL_ON_RNAQSET_Set(Var)   (Var).Set()
#define QUAL_ON_RNAQSET_Reset(Var) (Var).Reset()

/// RNAQSET_HAS_QUAL

#define RNAQSET_HAS_QUAL(Var) \
ITEM_HAS (QUAL_ON_RNAQSET, Var)

/// FOR_EACH_QUAL_ON_RNAQSET
/// EDIT_EACH_QUAL_ON_RNAQSET
// CRNA_qual_set& as input, dereference with [const] CRNA_qual& qual = **itr;

#define FOR_EACH_QUAL_ON_RNAQSET(Itr, Var) \
FOR_EACH (QUAL_ON_RNAQSET, Itr, Var)

#define EDIT_EACH_QUAL_ON_RNAQSET(Itr, Var) \
EDIT_EACH (QUAL_ON_RNAQSET, Itr, Var)

/// ADD_QUAL_TO_RNAQSET

#define ADD_QUAL_TO_RNAQSET(Var, Ref) \
ADD_ITEM (QUAL_ON_RNAQSET, Var, Ref)

/// ERASE_QUAL_ON_RNAQSET

#define ERASE_QUAL_ON_RNAQSET(Itr, Var) \
LIST_ERASE_ITEM (QUAL_ON_RNAQSET, Itr, Var)

/// QUAL_ON_RNAQSET_IS_SORTED

#define QUAL_ON_RNAQSET_IS_SORTED(Var, Func) \
IS_SORTED (QUAL_ON_RNAQSET, Var, Func)

/// SORT_QUAL_ON_RNAQSET

#define SORT_QUAL_ON_RNAQSET(Var, Func) \
DO_LIST_SORT (QUAL_ON_RNAQSET, Var, Func)

/// QUAL_ON_RNAQSET_IS_UNIQUE

#define QUAL_ON_RNAQSET_IS_UNIQUE(Var, Func) \
IS_UNIQUE (QUAL_ON_RNAQSET, Var, Func)

/// UNIQUE_QUAL_ON_RNAQSET

#define UNIQUE_QUAL_ON_RNAQSET(Var, Func) \
DO_UNIQUE (QUAL_ON_RNAQSET, Var, Func)

/// QUAL_ON_RNAQSET_IS_EMPTY

#define QUAL_ON_RNAQSET_IS_EMPTY(Var) \
    FIELD_IS_EMPTY(QUAL_ON_RNAQSET, Var)

/// REMOVE_IF_EMPTY_QUAL_ON_RNAQSET
#define REMOVE_IF_EMPTY_QUAL_ON_RNAQSET(Var) \
    REMOVE_IF_EMPTY_FIELD(QUAL_ON_RNAQSET, Var)

///
/// CTrna_ext macros

#define CODON_ON_TRNAEXT_Type       CTrna_ext::TCodon
#define CODON_ON_TRNAEXT_Test(Var)  (Var).IsSetCodon()
#define CODON_ON_TRNAEXT_Get(Var)   (Var).GetCodon()
#define CODON_ON_TRNAEXT_Set(Var)   (Var).SetCodon()
#define CODON_ON_TRNAEXT_Reset(Var) (Var).ResetCodon()

/// CODON_ON_TRNAEXT_IS_SORTED

#define CODON_ON_TRNAEXT_IS_SORTED(Var, Func) \
IS_SORTED (CODON_ON_TRNAEXT, Var, Func)

/// SORT_CODON_ON_TRNAEXT

#define SORT_CODON_ON_TRNAEXT(Var, Func) \
DO_LIST_SORT (CODON_ON_TRNAEXT, Var, Func)

/// CODON_ON_TRNAEXT_IS_UNIQUE

#define CODON_ON_TRNAEXT_IS_UNIQUE(Var, Func) \
IS_UNIQUE (CODON_ON_TRNAEXT, Var, Func)

/// UNIQUE_CODON_ON_TRNAEXT

#define UNIQUE_CODON_ON_TRNAEXT(Var, Func) \
DO_UNIQUE (CODON_ON_TRNAEXT, Var, Func)

/// CODON_ON_TRNAEXT_IS_EMPTY_OR_UNSET

#define CODON_ON_TRNAEXT_IS_EMPTY_OR_UNSET(Var) \
    FIELD_IS_EMPTY_OR_UNSET(CODON_ON_TRNAEXT, Var)

/// REMOVE_IF_EMPTY_CODON_ON_TRNAEXT

#define REMOVE_IF_EMPTY_CODON_ON_TRNAEXT(Var) \
    REMOVE_IF_EMPTY_FIELD(CODON_ON_TRNAEXT, Var)

///
/// CPCRParsedSet macros

#define PCRPARSEDSET_IN_LIST_Type       list<CPCRParsedSet>
#define PCRPARSEDSET_IN_LIST_Test(Var)  (! (Var).empty())
#define PCRPARSEDSET_IN_LIST_Get(Var)   (Var)
#define PCRPARSEDSET_IN_LIST_Set(Var)   (Var)
#define PCRPARSEDSET_IN_LIST_Reset(Var) (Var).clear()

#define FOR_EACH_PCRPARSEDSET_IN_LIST(Itr, Var) \
    FOR_EACH (PCRPARSEDSET_IN_LIST, Itr, Var)

///
/// CPCRReactionSet macros

#define PCRREACTION_IN_PCRREACTIONSET_Type       CPCRReactionSet::Tdata
#define PCRREACTION_IN_PCRREACTIONSET_Test(Var)  ( (Var).IsSet() && ! (Var).Get().empty() )
#define PCRREACTION_IN_PCRREACTIONSET_Get(Var)   (Var).Get()
#define PCRREACTION_IN_PCRREACTIONSET_Set(Var)   (Var).Set()
#define PCRREACTION_IN_PCRREACTIONSET_Reset(Var) (Var).Reset()

/// FOR_EACH_PCRREACTION_IN_PCRREACTIONSET

#define FOR_EACH_PCRREACTION_IN_PCRREACTIONSET(Itr, Var) \
    FOR_EACH (PCRREACTION_IN_PCRREACTIONSET, Itr, Var)

/// EDIT_EACH_PCRREACTION_IN_PCRREACTIONSET

#define EDIT_EACH_PCRREACTION_IN_PCRREACTIONSET(Itr, Var) \
    EDIT_EACH (PCRREACTION_IN_PCRREACTIONSET, Itr, Var)

/// ERASE_PCRREACTION_IN_PCRREACTIONSET

#define ERASE_PCRREACTION_IN_PCRREACTIONSET(Itr, Var) \
    LIST_ERASE_ITEM (PCRREACTION_IN_PCRREACTIONSET, Itr, Var)

/// REMOVE_IF_EMPTY_PCRREACTION_IN_PCRREACTIONSET

#define REMOVE_IF_EMPTY_PCRREACTION_IN_PCRREACTIONSET(Var) \
    REMOVE_IF_EMPTY_FIELD(PCRREACTION_IN_PCRREACTIONSET, Var)

/// UNIQUE_WITHOUT_SORT_PCRREACTION_IN_PCRREACTIONSET

#define UNIQUE_WITHOUT_SORT_PCRREACTION_IN_PCRREACTIONSET(Var, FuncType) \
UNIQUE_WITHOUT_SORT( PCRREACTION_IN_PCRREACTIONSET, Var, FuncType, \
    CCleanupChange::eChangePCRPrimers )

///
/// CPCRReaction macros

#define PCRPRIMER_IN_PCRPRIMERSET_Type       CPCRPrimerSet::Tdata
#define PCRPRIMER_IN_PCRPRIMERSET_Test(Var)  ( (Var).IsSet() && ! (Var).Get().empty() )
#define PCRPRIMER_IN_PCRPRIMERSET_Get(Var)   (Var).Get()
#define PCRPRIMER_IN_PCRPRIMERSET_Set(Var)   (Var).Set()
#define PCRPRIMER_IN_PCRPRIMERSET_Reset(Var) (Var).Reset()

/// FOR_EACH_PCRPRIMER_IN_PCRPRIMERSET

#define FOR_EACH_PCRPRIMER_IN_PCRPRIMERSET(Itr, Var) \
    FOR_EACH (PCRPRIMER_IN_PCRPRIMERSET, Itr, Var)

/// EDIT_EACH_PCRPRIMER_IN_PCRPRIMERSET

#define EDIT_EACH_PCRPRIMER_IN_PCRPRIMERSET(Itr, Var) \
    EDIT_EACH (PCRPRIMER_IN_PCRPRIMERSET, Itr, Var)

/// ERASE_PCRPRIMER_IN_PCRPRIMERSET

#define ERASE_PCRPRIMER_IN_PCRPRIMERSET(Itr, Var) \
    LIST_ERASE_ITEM (PCRPRIMER_IN_PCRPRIMERSET, Itr, Var)

/// UNIQUE_WITHOUT_SORT_PCRREACTION_IN_PCRREACTIONSET

#define UNIQUE_WITHOUT_SORT_PCRPRIMER_IN_PCRPRIMERSET(Var, FuncType) \
UNIQUE_WITHOUT_SORT( PCRPRIMER_IN_PCRPRIMERSET, Var, FuncType, \
    CCleanupChange::eChangePCRPrimers )

/// REMOVE_IF_EMPTY_PCRPRIMER_IN_PCRPRIMERSET

#define REMOVE_IF_EMPTY_PCRPRIMER_IN_PCRPRIMERSET(Var) \
    REMOVE_IF_EMPTY_FIELD(PCRPRIMER_IN_PCRPRIMERSET, Var)

///
/// CDelta_ext macros

#define DELTASEQ_IN_DELTAEXT_Type       list< CRef< CDelta_seq > >
#define DELTASEQ_IN_DELTAEXT_Test(Var)  (  (Var).IsSet() && ! (Var).Get().empty() )
#define DELTASEQ_IN_DELTAEXT_Get(Var)   (Var).Get()
#define DELTASEQ_IN_DELTAEXT_Set(Var)   (Var).Set()
#define DELTASEQ_IN_DELTAEXT_Reset(Var) (Var).Reset()

#define EDIT_EACH_DELTASEQ_IN_DELTAEXT(Itr, Var) \
    EDIT_EACH( DELTASEQ_IN_DELTAEXT, Itr, Var )

#define ERASE_DELTASEQ_IN_DELTAEXT(Itr, Var) \
    LIST_ERASE_ITEM (DELTASEQ_IN_DELTAEXT, Itr, Var)

///
/// @}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif /* __SEQUENCE_MACROS__HPP__ */

