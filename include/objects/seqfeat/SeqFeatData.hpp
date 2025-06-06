/* $Id$
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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the ASN data definition file
 *   'seqfeat.asn'.
 *
 * NOTE TO DEVELOPERS:
 *   The Seq-feat structure in 'seqfeat.asn' is subtyped into various
 *   more specialized structures by the content of the Seqfeat::data
 *   item. Depending on what is in the data item, other fields in
 *   Seq-feat (the ones marked optional in the ASN) are allowed or
 *   prohibited from being present. This file lists the possible
 *   subtypes of Seq-feat, along with the qualifiers that can occur
 *   within the Seq-feat structure.
 *
 * WHEN EDITING THE LIST OF QUALIFIERS (i.e. the EQualifier enum):
 * - add or remove the qualifier to the various lists that define which
 *   qualifiers are legal for a given subtype
 *   (in src/objects/seqfeat/SeqFeatData.cpp),
 * - add or remove the qualifier to the list of qualifiers known by the
 *   flatfile generator
 *   (in include/objtools/format/items/flat_qual_slots.hpp),
 * - add or remove processing code in the flat-file generator
 *   (src/objtools/format/feature_item.cpp). This may well necessitate
 *   the addition or removal of a class that knows how to format the
 *   new qualifier for display; look at the code for a similar
 *   established qualifier for an idea of what needs to be done
 *   (check WHEN EDITING THE LIST OF QUALIFIERS comment in that file
 *   for additional hints),
 * - add or remove the qualifier to the list of qualifiers known to
 *   the feature table reader (in src/objtools/readers/readfeat.cpp);
 *   it's an independent project with its own book-keeping concerning
 *   qualifiers, but needs to be kept in sync,
 * - make sure corresponding code gets added to the validator
 *   (in src/objtools/validator/...) which is another project with an
 *   independent qualifier list that nonetheless needs to stay in sync,
 * - (additional subitems to be added as I become aware of them).
 *
 * FEATURE REPRESENTATION:
 *   Although GenBank allows numerous kinds of features, certain types
 *   (Gene, Coding Region, Protein, and RNA) directly model the central
 *   dogma of molecular biology, and are designed to be used in making
 *   connections between records and in discovering new information by
 *   computation. Other features are stored as Import features, where
 *   the feature key is taken directly from the GenBank name. There are
 *   a few cases where the names can be confusing, and in each case the
 *   structured feature is normally used instead of the Import feature:
 *
 * - CDS uses eSubtype_cdregion instead of eSubtype_Imp_CDS.
 * - processed proteins use the "_aa" versions:
 *     eSubtype_mat_peptide_aa instead of eSubtype_mat_peptide.
 *     eSubtype_sig_peptide_aa instead of eSubtype_sig_peptide.
 *     eSubtype_transit_peptide_aa instead of eSubtype_transit_peptide.
 * - misc_RNA uses eSubtype_otherRNA instead of eSubtype_misc_RNA.
 * - precursor_RNA uses eSubtype_preRNA instead of eSubtype_precursor_RNA.
 *
 */

#ifndef OBJECTS_SEQFEAT_SEQFEATDATA_HPP
#define OBJECTS_SEQFEAT_SEQFEATDATA_HPP


// generated includes
#include <objects/seqfeat/SeqFeatData_.hpp>
#include <set>

#include <corelib/ncbistr.hpp>
#include <util/static_map.hpp>
#include <array>
#include <util/compile_time.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class CFeatList;
class CBondList;
class CSiteList;
class CGb_qual;

class NCBI_SEQFEAT_EXPORT CSeqFeatData : public CSeqFeatData_Base
{
    typedef CSeqFeatData_Base Tparent;
public:
    /// constructor
    CSeqFeatData(void);
    /// destructor
    ~CSeqFeatData(void);

    /// ASCII representation of subtype (GenBank feature key, e.g.)
    enum EVocabulary {
        eVocabulary_full,
        eVocabulary_genbank
    };
    string GetKey(EVocabulary vocab = eVocabulary_full) const;

    enum ESubtype {
        /// These no longer need to match the FEATDEF values in the
        /// C toolkit's objfdef.h
        eSubtype_bad                = 0,
        eSubtype_gene               = 1,
        eSubtype_org                = 2,
        eSubtype_cdregion           = 3,
        eSubtype_prot               = 4,
        eSubtype_preprotein         = 5,
        eSubtype_mat_peptide_aa     = 6,  // Prot-ref mat_peptide
        eSubtype_sig_peptide_aa     = 7,  // Prot-ref sig_peptide
        eSubtype_transit_peptide_aa = 8,  // Prot-ref transit_peptide
        eSubtype_preRNA             = 9,  // RNA-ref precursor_RNA
        eSubtype_mRNA               = 10,
        eSubtype_tRNA               = 11,
        eSubtype_rRNA               = 12,
        eSubtype_snRNA              = 13,
        eSubtype_scRNA              = 14,
        eSubtype_snoRNA             = 15,
        eSubtype_otherRNA           = 16, // RNA-ref misc_RNA
        eSubtype_pub                = 17,
        eSubtype_seq                = 18,
        eSubtype_imp                = 19,
        eSubtype_allele             = 20,
        eSubtype_attenuator         = 21,
        eSubtype_C_region           = 22,
        eSubtype_CAAT_signal        = 23,
        eSubtype_Imp_CDS            = 24, // use eSubtype_cdregion for CDS
        eSubtype_conflict           = 25,
        eSubtype_D_loop             = 26,
        eSubtype_D_segment          = 27,
        eSubtype_enhancer           = 28,
        eSubtype_exon               = 29,
        eSubtype_EC_number          = 30,
        eSubtype_GC_signal          = 31,
        eSubtype_iDNA               = 32,
        eSubtype_intron             = 33,
        eSubtype_J_segment          = 34,
        eSubtype_LTR                = 35,
        eSubtype_mat_peptide        = 36, // use eSubtype_mat_peptide_aa
        eSubtype_misc_binding       = 37,
        eSubtype_misc_difference    = 38,
        eSubtype_misc_feature       = 39,
        eSubtype_misc_recomb        = 40,
        eSubtype_misc_RNA           = 41, // use eSubtype_otherRNA for misc_RNA
        eSubtype_misc_signal        = 42,
        eSubtype_misc_structure     = 43,
        eSubtype_modified_base      = 44,
        eSubtype_mutation           = 45,
        eSubtype_N_region           = 46,
        eSubtype_old_sequence       = 47,
        eSubtype_polyA_signal       = 48,
        eSubtype_polyA_site         = 49,
        eSubtype_precursor_RNA      = 50, // use eSubtype_preRNA for precursor_RNA
        eSubtype_prim_transcript    = 51,
        eSubtype_primer_bind        = 52,
        eSubtype_promoter           = 53,
        eSubtype_protein_bind       = 54,
        eSubtype_RBS                = 55,
        eSubtype_repeat_region      = 56,
        eSubtype_repeat_unit        = 57,
        eSubtype_rep_origin         = 58,
        eSubtype_S_region           = 59,
        eSubtype_satellite          = 60,
        eSubtype_sig_peptide        = 61, // use eSubtype_sig_peptide_aa
        eSubtype_source             = 62,
        eSubtype_stem_loop          = 63,
        eSubtype_STS                = 64,
        eSubtype_TATA_signal        = 65,
        eSubtype_terminator         = 66,
        eSubtype_transit_peptide    = 67, // use eSubtype_transit_peptide_aa
        eSubtype_unsure             = 68,
        eSubtype_V_region           = 69,
        eSubtype_V_segment          = 70,
        eSubtype_variation          = 71, //< old variation (data.imp)
        eSubtype_virion             = 72,
        eSubtype_3clip              = 73,
        eSubtype_3UTR               = 74,
        eSubtype_5clip              = 75,
        eSubtype_5UTR               = 76,
        eSubtype_10_signal          = 77,
        eSubtype_35_signal          = 78,
        eSubtype_gap                = 79,
        eSubtype_operon             = 80,
        eSubtype_oriT               = 81,
        eSubtype_site_ref           = 82,
        eSubtype_region             = 83,
        eSubtype_comment            = 84,
        eSubtype_bond               = 85,
        eSubtype_site               = 86,
        eSubtype_rsite              = 87,
        eSubtype_user               = 88,
        eSubtype_txinit             = 89,
        eSubtype_num                = 90,
        eSubtype_psec_str           = 91,
        eSubtype_non_std_residue    = 92,
        eSubtype_het                = 93,
        eSubtype_biosrc             = 94,
        eSubtype_ncRNA              = 95,
        eSubtype_tmRNA              = 96,
        eSubtype_clone              = 97,
        eSubtype_variation_ref      = 98, //< new variation (data.variant)
        eSubtype_mobile_element     = 99,
        eSubtype_centromere         = 100,
        eSubtype_telomere           = 101,
        eSubtype_assembly_gap       = 102,
        eSubtype_regulatory         = 103,
        eSubtype_propeptide         = 104,   // use eSubtype_propeptide_aa
        eSubtype_propeptide_aa      = 105,  // Prot-ref propeptide
        eSubtype_max                = 106,
        eSubtype_any                = 255
    };
    ESubtype GetSubtype(void) const;

    /// Call after changes that could affect the subtype
    void InvalidateSubtype(void) const;

    /// Combine invalidation of all cached values
    void InvalidateCache(void) const;

    /// Override all setters to incorporate cache invalidation.
#define DEFINE_NCBI_SEQFEATDATA_VAL_SETTERS(x)  \
    void Set##x(const T##x& v) {                \
        InvalidateCache();                      \
        Tparent::Set##x(v);                     \
    }                                           \
    T##x& Set##x(void) {                        \
        InvalidateCache();                      \
        return Tparent::Set##x();               \
    }
#define DEFINE_NCBI_SEQFEATDATA_SETTERS(x)      \
    void Set##x(T##x& v) {                      \
        InvalidateCache();                      \
        Tparent::Set##x(v);                     \
    }                                           \
    T##x& Set##x(void) {                        \
        InvalidateCache();                      \
        return Tparent::Set##x();               \
    }

    DEFINE_NCBI_SEQFEATDATA_SETTERS(Gene)
    DEFINE_NCBI_SEQFEATDATA_SETTERS(Org)
    DEFINE_NCBI_SEQFEATDATA_SETTERS(Cdregion)
    DEFINE_NCBI_SEQFEATDATA_SETTERS(Prot)
    DEFINE_NCBI_SEQFEATDATA_SETTERS(Rna)
    DEFINE_NCBI_SEQFEATDATA_SETTERS(Pub)
    DEFINE_NCBI_SEQFEATDATA_SETTERS(Seq)
    DEFINE_NCBI_SEQFEATDATA_SETTERS(Imp)
    DEFINE_NCBI_SEQFEATDATA_VAL_SETTERS(Region)
    void SetComment(void) {
        InvalidateCache();
        Tparent::SetComment();
    }
    DEFINE_NCBI_SEQFEATDATA_VAL_SETTERS(Bond)
    DEFINE_NCBI_SEQFEATDATA_VAL_SETTERS(Site)
    DEFINE_NCBI_SEQFEATDATA_SETTERS(Rsite)
    DEFINE_NCBI_SEQFEATDATA_SETTERS(User)
    DEFINE_NCBI_SEQFEATDATA_SETTERS(Txinit)
    DEFINE_NCBI_SEQFEATDATA_SETTERS(Num)
    DEFINE_NCBI_SEQFEATDATA_VAL_SETTERS(Psec_str)
    DEFINE_NCBI_SEQFEATDATA_VAL_SETTERS(Non_std_residue)
    DEFINE_NCBI_SEQFEATDATA_SETTERS(Het)
    DEFINE_NCBI_SEQFEATDATA_SETTERS(Biosrc)
    DEFINE_NCBI_SEQFEATDATA_SETTERS(Clone)
    DEFINE_NCBI_SEQFEATDATA_SETTERS(Variation)

#undef DEFINE_NCBI_SEQFEATDATA_SETTERS
#undef DEFINE_NCBI_SEQFEATDATA_VAL_SETTERS

    /// Override Assign() to incorporate cache invalidation.
    virtual void Assign(const CSerialObject& source,
                        ESerialRecursionMode how = eRecursive);

    /// Invalidate subtype cache after deserialization
    void PostRead(void) const;

    static E_Choice GetTypeFromSubtype (ESubtype subtype);
    /// Turns a ESubtype into its string value which is NOT necessarily
    /// related to the identifier of the enum.
    ///
    /// @return
    ///   empty string on bad input
    static CTempString SubtypeValueToName(ESubtype eSubtype);
    /// Turn a string into its ESubtype which is NOT necessarily
    /// related to the identifier of the enum.
    ///
    /// @return
    ///   eSubtype_bad on bad input
    static ESubtype SubtypeNameToValue(CTempString sName);


    static bool CanHaveGene(ESubtype subtype);

    /// List of available qualifiers for feature keys.
    /// For more information see:
    ///   The DDBJ/EMBL/GenBank Feature Table: Definition
    ///   (http://www.ncbi.nlm.nih.gov/projects/collab/FT/index.html)
    enum EQualifier
    {
        eQual_bad = 0,
        eQual_allele,
        eQual_altitude,
        eQual_anticodon,
        eQual_artificial_location,
        eQual_bio_material,
        eQual_bond_type,
        eQual_bound_moiety,
        eQual_calculated_mol_wt,
        eQual_cell_line,
        eQual_cell_type,
        eQual_chloroplast,
        eQual_chromoplast,
        eQual_chromosome,
        eQual_circular_RNA,
        eQual_citation,
        eQual_clone,
        eQual_clone_lib,
        eQual_coded_by,
        eQual_codon,
        eQual_codon_start,
        eQual_collected_by,
        eQual_collection_date,
        eQual_compare,
        eQual_cons_splice,
        eQual_country,
        eQual_cultivar,
        eQual_culture_collection,
        eQual_cyanelle,
        eQual_db_xref,
        eQual_derived_from,
        eQual_dev_stage,
        eQual_direction,
        eQual_EC_number,
        eQual_ecotype,
        eQual_environmental_sample,
        eQual_estimated_length,
        eQual_evidence,
        eQual_exception,
        eQual_experiment,
        eQual_feat_class,
        eQual_focus,
        eQual_frequency,
        eQual_function,
        eQual_gap_type,
        eQual_gdb_xref,
        eQual_gene,
        eQual_gene_synonym,
        eQual_germline,
        eQual_haplogroup,
        eQual_haplotype,
        eQual_heterogen,
        eQual_host,
        eQual_identified_by,
        eQual_inference,
        eQual_insertion_seq,
        eQual_isolate,
        eQual_isolation_source,
        eQual_kinetoplast,
        eQual_lab_host,
        eQual_label,
        eQual_lat_lon,
        eQual_linkage_evidence,
        eQual_linkage_group,
        eQual_locus_tag,
        eQual_macronuclear,
        eQual_map,
        eQual_mating_type,
        eQual_metagenome_source,
        eQual_metagenomic,
        eQual_mitochondrion,
        eQual_mobile_element,
        eQual_mobile_element_type,
        eQual_mod_base,
        eQual_mol_type,
        eQual_name,
        eQual_nomenclature,
        eQual_non_std_residue,
        eQual_ncRNA_class,
        eQual_note,
        eQual_number,
        eQual_old_locus_tag,
        eQual_operon,
        eQual_organelle,
        eQual_organism,
        eQual_partial,
        eQual_PCR_conditions,
        eQual_PCR_primers,
        eQual_phenotype,
        eQual_plasmid,
        eQual_pop_variant,
        eQual_product,
        eQual_protein_id,
        eQual_proviral,
        eQual_pseudo,
        eQual_pseudogene,
        eQual_rearranged,
        eQual_recombination_class,
        eQual_region_name,
        eQual_regulatory_class,
        eQual_replace,
        eQual_ribosomal_slippage,
        eQual_rpt_family,
        eQual_rpt_type,
        eQual_rpt_unit,
        eQual_rpt_unit_range,
        eQual_rpt_unit_seq,
        eQual_satellite,
        eQual_sec_str_type,
        eQual_segment,
        eQual_sequenced_mol,
        eQual_serotype,
        eQual_serovar,
        eQual_sex,
        eQual_site_type,
        eQual_SO_type,
        eQual_specimen_voucher,
        eQual_standard_name,
        eQual_strain,
        eQual_submitter_seqid,
        eQual_sub_clone,
        eQual_sub_species,
        eQual_sub_strain,
        eQual_tag_peptide,
        eQual_tissue_lib,
        eQual_tissue_type,
        eQual_trans_splicing,
        eQual_transcript_id,
        eQual_transgenic,
        eQual_translation,
        eQual_transl_except,
        eQual_transl_table,
        eQual_transposon,
        eQual_type_material,
        eQual_UniProtKB_evidence,
        eQual_usedin,
        eQual_variety,
        eQual_virion,
        eQual_whole_replicon
    };

    using TLegalQualifiers = ct::const_bitset<eQual_whole_replicon + 1, EQualifier>;
    using TSubtypes = ct::const_bitset<eSubtype_max, ESubtype>;
    using TQualifiers = TLegalQualifiers;
    using TSubTypeQualifiersMap = ct::const_map<ESubtype, TQualifiers>;

    /// Test wheather a certain qualifier is legal for the feature
    bool IsLegalQualifier(EQualifier qual) const;
    static bool IsLegalQualifier(ESubtype subtype, EQualifier qual);

    /// Get a list of all the legal qualifiers for the feature.
    const TLegalQualifiers& GetLegalQualifiers(void) const;
    static const TLegalQualifiers& GetLegalQualifiers(ESubtype subtype);

    /// Get the list of all mandatory qualifiers for the feature.
    const TQualifiers& GetMandatoryQualifiers(void) const;
    static const TQualifiers& GetMandatoryQualifiers(ESubtype subtype);

    /// Convert a qualifier from an enumerated value to a string representation
    /// or empty if not found.
    static CTempString GetQualifierAsString(EQualifier qual);

    /// convert qual string to enumerated value
    static EQualifier GetQualifierType(CTempString qual);
    static std::pair<EQualifier, CTempString> GetQualifierTypeAndValue(CTempString qual);

    static const CFeatList* GetFeatList();
    static const CBondList* GetBondList();
    static const CSiteList* GetSiteList();

    static bool IsRegulatory(ESubtype subtype);
    static const string & GetRegulatoryClass(ESubtype subtype);
    static ESubtype GetRegulatoryClass(const string & class_name );
    static const vector<string>& GetRegulatoryClassList();
    static bool FixRegulatoryClassValue(string& val);

    static const vector<string>& GetRecombinationClassList();

    static bool IsDiscouragedSubtype(ESubtype subtype);
    static bool IsDiscouragedQual(EQualifier qual);

    enum EFeatureLocationAllowed {
        eFeatureLocationAllowed_Any = 0,
        eFeatureLocationAllowed_Error,
        eFeatureLocationAllowed_NucOnly,
        eFeatureLocationAllowed_ProtOnly,
    };

    static EFeatureLocationAllowed AllowedFeatureLocation(ESubtype subtype);

    static bool AllowStrandBoth(ESubtype subtype);
    static bool RequireLocationIntervalsInBiologicalOrder(ESubtype subtype);
    static bool AllowAdjacentIntervals(ESubtype subtype);

    static bool ShouldRepresentAsGbqual (CSeqFeatData::ESubtype feat_subtype, const CGb_qual& qual);
    static bool ShouldRepresentAsGbqual (CSeqFeatData::ESubtype feat_subtype, CSeqFeatData::EQualifier qual_type);

    static bool AllowXref(CSeqFeatData::ESubtype subtype1, CSeqFeatData::ESubtype subtype2);
    static bool ProhibitXref(CSeqFeatData::ESubtype subtype1, CSeqFeatData::ESubtype subtype2);

    static bool FixImportKey(string& key);
    static bool IsLegalProductNameForRibosomalSlippage(const string& product_name);
    // Internal structure to hold additional info
    struct SFeatDataInfo
    {
        CSeqFeatData::ESubtype m_Subtype;
        const char*            m_Key_full;
        const char*            m_Key_gb;
    };

private:
    void x_InitFeatDataInfo(void) const;

    mutable SFeatDataInfo m_FeatDataInfo; // cached

    static void s_InitSubtypesTable(void);
    static const TSubTypeQualifiersMap& s_GetLegalQualMap() noexcept;

    // Prohibit copy constructor and assignment operator
    CSeqFeatData(const CSeqFeatData& value);
    CSeqFeatData& operator=(const CSeqFeatData& value);

    typedef CStaticArraySet<ESubtype> TSubtypeSet;
    static const TSubtypeSet & GetSetOfRegulatorySubtypes();
};


/////////////////// CSeqFeatData inline methods

inline
void CSeqFeatData::InvalidateSubtype(void) const
{
    m_FeatDataInfo.m_Subtype = eSubtype_any;
}


inline
void CSeqFeatData::InvalidateCache(void) const
{
    InvalidateSubtype(); // it's enough to invalidate all cached values
}


inline
bool CSeqFeatData::IsLegalQualifier(EQualifier qual) const
{
    return IsLegalQualifier(GetSubtype(), qual);
}


inline
const CSeqFeatData::TLegalQualifiers& CSeqFeatData::GetLegalQualifiers(void) const
{
    return GetLegalQualifiers(GetSubtype());
}


inline
const CSeqFeatData::TQualifiers& CSeqFeatData::GetMandatoryQualifiers(void) const
{
    return GetMandatoryQualifiers(GetSubtype());
}

/////////////////// end of CSeqFeatData inline methods




/// CFeatListItem - basic configuration data for one "feature" type.
/// "feature" expanded to include things that are not SeqFeats.
class NCBI_SEQFEAT_EXPORT CFeatListItem
{
public:
    CFeatListItem() {}
    CFeatListItem(int type, int subtype, const char* desc, const char* key)
        : m_Type(type)
        , m_Subtype(subtype)
        , m_Description(desc)
        , m_StorageKey(key) {}

    bool operator<(const CFeatListItem& rhs) const;

    int         GetType() const;
    int         GetSubtype() const;
    string      GetDescription() const;
    string      GetStoragekey() const;

private:
    int         m_Type = 0;         ///< Feature type, or e_not_set for default values.
    int         m_Subtype = 0;      ///< Feature subtype or eSubtype_any for default values.
    string      m_Description;  ///< a string for display purposes.
    string      m_StorageKey;   ///< a short string to use as a key or part of a key
                                ///< when storing a value by key/value string pairs.
};


// Inline methods.

inline
int CFeatListItem::GetType() const
{ return m_Type; }

inline
int CFeatListItem::GetSubtype() const
{ return m_Subtype; }

inline
string CFeatListItem::GetDescription() const
{ return m_Description; }

inline
string CFeatListItem::GetStoragekey() const
{ return m_StorageKey; }


/// CConfigurableItems - a static list of items that can be configured.
///
/// One can iterate through all items. Iterators dereference to CFeatListItem.
/// One can also get an item by type and subtype.

class NCBI_SEQFEAT_EXPORT CFeatList
{
private:
    typedef set<CFeatListItem>    TFeatTypeContainer;

public:

    CFeatList();
    ~CFeatList();

    bool    TypeValid(int type, int subtype) const;

    /// can get all static information for one type/subtype.
    bool    GetItem(int type, int subtype, CFeatListItem& config_item) const;
    bool    GetItemBySubtype(int subtype,  CFeatListItem& config_item) const;

    bool    GetItemByDescription(const string& desc, CFeatListItem& config_item) const;
    bool    GetItemByKey(const string& key, CFeatListItem& config_item) const;

    /// Get the displayable description of this type of feature.
    string      GetDescription(int type, int subtype) const;

    /// Get the feature's type and subtype from its description.
    /// return true on success, false if type and subtype are not valid.
    bool        GetTypeSubType(const string& desc, int& type, int& subtype) const;

    /// Get the key used to store this type of feature.
    string      GetStoragekey(int type, int subtype) const;
    /// Get the key used to store this type of feature by only subtype.
    string      GetStoragekey(int subtype) const;

    /// Get hierarchy of keys above this subtype, starting with "Master"
    /// example, eSubtype_gene, will return {"Master", "Gene"}
    /// but eSubtype_allele will return {"Master", "Import", "allele"}
    vector<string>  GetStoragekeys(int subtype) const;

    /// return a list of all the feature descriptions for a menu or other control.
    /// if hierarchical is true use in an Fl_Menu_ descendant to make hierarchical menus.
    void    GetDescriptions(
                            vector<string> &descs,          ///> output, list of description strings.
                            bool hierarchical = false       ///> make descriptions hierachical, separated by '/'.
                            ) const;

    // Iteratable list of key values (type/subtype).
    // can iterate through all values including defaults or with only
    // real Feature types/subtypes.
    typedef TFeatTypeContainer::const_iterator const_iterator;

    size_t          size() const;
    const_iterator  begin() const;
    const_iterator  end() const;
private:
        /// initialize our container of feature types and descriptions.
        void    x_Init(void);

    TFeatTypeContainer    m_FeatTypes;    ///> all valid types and subtypes.

    typedef map<int, CFeatListItem>   TSubtypeMap;
    TSubtypeMap    m_FeatTypeMap; ///> indexed by subtype only.

};


inline
size_t CFeatList::size() const
{
    return m_FeatTypes.size();
}


inline
CFeatList::const_iterator CFeatList::begin() const
{
    return m_FeatTypes.begin();
}


inline
CFeatList::const_iterator CFeatList::end() const
{
    return m_FeatTypes.end();
}


class NCBI_SEQFEAT_EXPORT CBondList
{
public:
    typedef SStaticPair<const char *, CSeqFeatData::EBond> TBondKey;

private:
    typedef CStaticPairArrayMap<const char*, CSeqFeatData::EBond, PNocase_CStr> TBondMap;

public:

    CBondList();
    ~CBondList();

    bool    IsBondName (string str) const;
    bool    IsBondName (string str, CSeqFeatData::EBond& bond_type) const;
    CSeqFeatData::EBond GetBondType (string str) const;

    // Iteratable list of key values (type/subtype).
    // can iterate through all values including defaults or with only
    // real Feature types/subtypes.
    typedef TBondMap::const_iterator const_iterator;

    size_t          size() const;
    const_iterator  begin() const;
    const_iterator  end() const;
private:
        /// initialize our container of feature types and descriptions.
        void    x_Init(void);

    DECLARE_CLASS_STATIC_ARRAY_MAP(TBondMap, sm_BondKeys);

};


inline
size_t CBondList::size() const
{
    return sm_BondKeys.size();
}


inline
CBondList::const_iterator CBondList::begin() const
{
    return sm_BondKeys.begin();
}


inline
CBondList::const_iterator CBondList::end() const
{
    return sm_BondKeys.end();
}


class NCBI_SEQFEAT_EXPORT CSiteList
{
public:
    typedef SStaticPair<const char *, CSeqFeatData::ESite> TSiteKey;

private:
    typedef CStaticPairArrayMap<const char*, CSeqFeatData::ESite, PNocase_CStr> TSiteMap;

public:

    CSiteList();
    ~CSiteList();

    bool    IsSiteName (string str) const;
    bool    IsSiteName (string str, CSeqFeatData::ESite& site_type) const;
    CSeqFeatData::ESite GetSiteType (string str) const;

    // Iteratable list of key values (type/subtype).
    // can iterate through all values including defaults or with only
    // real Feature types/subtypes.
    typedef TSiteMap::const_iterator const_iterator;

    size_t          size() const;
    const_iterator  begin() const;
    const_iterator  end() const;
private:
        /// initialize our container of feature types and descriptions.
        void    x_Init(void);

    DECLARE_CLASS_STATIC_ARRAY_MAP(TSiteMap, sm_SiteKeys);

};


inline
size_t CSiteList::size() const
{
    return sm_SiteKeys.size();
}


inline
CSiteList::const_iterator CSiteList::begin() const
{
    return sm_SiteKeys.begin();
}


inline
CSiteList::const_iterator CSiteList::end() const
{
    return sm_SiteKeys.end();
}

NCBISER_HAVE_POST_READ(CSeqFeatData)


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // OBJECTS_SEQFEAT_SEQFEATDATA_HPP

/* Original file checksum: lines: 90, chars: 2439, CRC32: 742431cc */
