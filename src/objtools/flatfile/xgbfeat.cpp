/***************************************************************************
 *   gbfeat.c:
 *   -- all routines for checking genbank feature table
 *   -- all extern variables are in gbftglob.c
 *                                                                  10-11-93
 *
 ****************************************************************************/

#include <ncbi_pch.hpp>

#include <objects/seqfeat/SeqFeatData.hpp>
#include <objtools/cleanup/cleanup.hpp>

#include "ftacpp.hpp"

#include "ftaerr.hpp"
#include "xgbfeat.h"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "xgbfeat.cpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

static const Char* this_module = "validatr";

#ifdef THIS_MODULE
#  undef THIS_MODULE
#endif
#define THIS_MODULE this_module


// error definitions from C-toolkit
#define ERR_FEATURE                     1, 0
#define ERR_FEATURE_UnknownFeatureKey   1, 1
#define ERR_FEATURE_MissManQual         1, 2
#define ERR_FEATURE_QualWrongThisFeat   1, 3
#define ERR_FEATURE_FeatureKeyReplaced  1, 4
#define ERR_FEATURE_LocationParsing     1, 5
#define ERR_FEATURE_IllegalFormat       1, 6
#define ERR_QUALIFIER                   2, 0
#define ERR_QUALIFIER_InvalidDataFormat 2, 1
#define ERR_QUALIFIER_Too_many_tokens   2, 2
#define ERR_QUALIFIER_MultiValue        2, 3
#define ERR_QUALIFIER_UnknownSpelling   2, 4
#define ERR_QUALIFIER_Xtratext          2, 5
#define ERR_QUALIFIER_SeqPosComma       2, 6
#define ERR_QUALIFIER_Pos               2, 7
#define ERR_QUALIFIER_EmptyNote         2, 8
#define ERR_QUALIFIER_NoteEmbeddedQual  2, 9
#define ERR_QUALIFIER_EmbeddedQual      2, 10
#define ERR_QUALIFIER_AA                2, 11
#define ERR_QUALIFIER_Seq               2, 12
#define ERR_QUALIFIER_BadECnum          2, 13
#define ERR_QUALIFIER_Cons_splice       2, 14

#define ParFlat_Stoken_type     1
#define ParFlat_BracketInt_type 2
#define ParFlat_Integer_type    3
#define ParFlat_Number_type     4

static int SplitMultiValQual(TQualVector& quals);
static int GBQualSemanticValid(TQualVector& quals, bool error_msgs, bool perform_corrections);
static int CkQualPosaa(CGb_qual& cur, bool error_msgs);
static int CkQualNote(CGb_qual& cur,
                      bool      error_msgs,
                      bool      perform_corrections);
static int CkQualText(CGb_qual& cur,
                      bool*     has_embedded,
                      bool      from_note,
                      bool      error_msgs,
                      bool      perform_corrections);
static int CkQualTokenType(CGb_qual& cur,
                           bool      error_msgs,
                           Uint1     type);
static int CkQualSeqaa(CGb_qual& cur, bool error_msgs);
static int CkQualMatchToken(CGb_qual&   cur,
                            bool        error_msgs,
                            const Char* array_string[],
                            Int2        totalstr);
static int CkQualSite(CGb_qual& cur, bool error_msgs);
static int CkQualEcnum(CGb_qual& cur,
                       bool      error_msgs,
                       bool      perform_corrections);

static const Char* CkBracketType(const Char* str);
static const Char* CkNumberType(const Char* str);
static const Char* CkLabelType(const Char* str);


#define ParFlat_SPLIT_IGNORE 4
const Char* GBQual_names_split_ignore[ParFlat_SPLIT_IGNORE] = {
    "citation", "EC_number", "rpt_type", "usedin"
};


/*------------------------- GBQualSplit() ------------------------*/
/****************************************************************************
 *  GBQualSplit:
 *  -- return index of the GBQual_names_split_ignore array if it is a valid
 *     qualifier (ignore case), qual; otherwise return (-1)
 *                                                                   10-12-93
 *****************************************************************************/
static Int2 GBQualSplit(const Char* qual)
{
    Int2 i;

    for (i = 0; i < ParFlat_SPLIT_IGNORE && qual; i++) {
        if (NStr::EqualNocase(qual, GBQual_names_split_ignore[i]))
            return (i);
    }

    return (-1);

} /* GBQualSplit */

/*--------------------------- GBFeatKeyQualValid() -----------------------*/
/***************************************************************************
 *  GBFeatKeyQualValid:
 *  -- returns error severity level.
 *    error dealt with here.  Messages output if parameter 'error_msgs' set,
 *    repair done if 'perform_corrections' set
 *                                                                   10-11-93
 *****************************************************************************/
int XGBFeatKeyQualValid(CSeqFeatData::ESubtype subtype, TQualVector& quals, bool error_msgs, bool perform_corrections)
{
    bool fqual  = false;
    int  retval = GB_FEAT_ERR_NONE;

    /* unknown qual will be drop after the routine */
    retval = SplitMultiValQual(quals);
    retval = GBQualSemanticValid(quals, error_msgs, perform_corrections);
    /*----------------------------------------
         if the Semnatic QUALIFIER validator says drop, then
         at the feature level, it is repairable by dropping the
         qualifier.  The only DROP for a feature is lack of
         a manditory qualifier which is handled later in this function.
         -Karl 2/7/94
         -----------------------------*/
    if (retval == GB_FEAT_ERR_DROP) {
        retval = GB_FEAT_ERR_REPAIRABLE;
    }

    for (TQualVector::iterator cur = quals.begin(); cur != quals.end();) {
        const string& qual_str = (*cur)->GetQual();
        if (qual_str == "gsdb_id") {
            ++cur;
            continue;
        }

        string qs = qual_str;
        if (qs == "geo_loc_name") {
            qs = "country";
        }
        CSeqFeatData::EQualifier qual_type = CSeqFeatData::GetQualifierType(qs);
        fqual                              = CSeqFeatData::IsLegalQualifier(subtype, qual_type);

        if (! fqual) {
            /* go back to check, is this a mandatory qualifier ? */
            for (CSeqFeatData::EQualifier cur_type : CSeqFeatData::GetMandatoryQualifiers(subtype)) {
                if (qual_type == cur_type) {
                    fqual = true;
                    break;
                }
            }

            if (! fqual) {
                if (retval < GB_FEAT_ERR_REPAIRABLE) {
                    retval = GB_FEAT_ERR_REPAIRABLE;
                }
                if (error_msgs) {
                    FtaErrPost(SEV_ERROR, ERR_FEATURE_QualWrongThisFeat, qs);
                }
                if (perform_corrections) {
                    cur = quals.erase(cur);
                    continue;
                }
            }
        }

        ++cur;
    }

    if (! CSeqFeatData::GetMandatoryQualifiers(subtype).empty()) {
        /* do they contain all the mandatory qualifiers? */
        for (CSeqFeatData::EQualifier cur_type : CSeqFeatData::GetMandatoryQualifiers(subtype)) {
            for (const auto& cur : quals) {
                fqual = false;

                if (cur_type == CSeqFeatData::GetQualifierType(cur->GetQual())) {
                    fqual = true;
                    break;
                }
            }

            if (! fqual) {
                if (error_msgs) {
                    string str = CSeqFeatData::GetQualifierAsString(cur_type);
                    FtaErrPost(SEV_ERROR, ERR_FEATURE_MissManQual, str);
                }

                if (perform_corrections)
                    retval = GB_FEAT_ERR_DROP;
            }
        }
    }
    /* check optional qualifiers */

    return retval;

} /* GBFeatKeyQualValid */

/*-------------------------- SplitMultiValQual() ------------------------*/
/***************************************************************************
 *  SplitMultiValQual:
 *
 *
 ****************************************************************************/
static int SplitMultiValQual(TQualVector& quals)
{
    Int2 val /*, len -- UNUSED */;
    int  retval = GB_FEAT_ERR_NONE;

    for (auto& cur : quals) {
        const string& qual_str = cur->GetQual();
        val                    = GBQualSplit(qual_str.c_str());

        if (val == -1) {
            continue;
        }
        if (! cur->IsSetVal()) {
            continue;
        }

        string val_str = cur->GetVal();
        if (*val_str.begin() != '(') {
            continue;
        }
        if (*val_str.rbegin() != ')') {
            continue;
        }

        val_str        = val_str.substr(1, val_str.size() - 2);
        size_t sep_pos = val_str.find(',');
        if (sep_pos == string::npos) {
            cur->SetVal(val_str);
            continue;
        }

        FtaErrPost(SEV_WARNING, ERR_QUALIFIER_MultiValue, "Splited qualifier {}", qual_str);

        cur->SetVal(val_str.substr(0, sep_pos));

        size_t offset = sep_pos + 1;
        sep_pos       = val_str.find(',', offset);
        while (sep_pos != string::npos) {
            CRef<CGb_qual> qual_new(new CGb_qual);
            qual_new->SetQual(qual_str);
            qual_new->SetVal(val_str.substr(offset, sep_pos - offset));

            if (qual_new->IsSetQual() && ! qual_new->GetQual().empty())
                quals.push_back(qual_new);
        }
    }

    return retval;

} /* SplitMultiValQual */

enum ETokenClass {
    eClass_pos_aa = 1,
    eClass_text,
    eClass_bracket_int,
    eClass_seq_aa,
    eClass_int_or,
    eClass_site,
    eClass_L_R_B,
    eClass_ecnum,
    eClass_exper,
    eClass_none,
    eClass_token,
    eClass_int,
    eClass_rpt,
    eClass_flabel_base,
    eClass_flabel_dbname,
    eClass_note,
    eClass_number,
    eClass_unknown
};

static ETokenClass GetQualifierClass(CSeqFeatData::EQualifier qual_type)
{
    static map<CSeqFeatData::EQualifier, ETokenClass> QUAL_TYPE_TO_VAL_CLASS_MAP = {
        { CSeqFeatData::eQual_bad, eClass_none },
        { CSeqFeatData::eQual_allele, eClass_text },
        { CSeqFeatData::eQual_altitude, eClass_text },
        { CSeqFeatData::eQual_anticodon, eClass_pos_aa },
        { CSeqFeatData::eQual_artificial_location, eClass_text },
        { CSeqFeatData::eQual_bio_material, eClass_text },
        { CSeqFeatData::eQual_bond_type, eClass_none },
        { CSeqFeatData::eQual_bound_moiety, eClass_text },
        { CSeqFeatData::eQual_calculated_mol_wt, eClass_none },
        { CSeqFeatData::eQual_cell_line, eClass_text },
        { CSeqFeatData::eQual_cell_type, eClass_text },
        { CSeqFeatData::eQual_chloroplast, eClass_none },
        { CSeqFeatData::eQual_chromoplast, eClass_none },
        { CSeqFeatData::eQual_chromosome, eClass_text },
        { CSeqFeatData::eQual_citation, eClass_bracket_int },
        { CSeqFeatData::eQual_clone, eClass_text },
        { CSeqFeatData::eQual_clone_lib, eClass_text },
        { CSeqFeatData::eQual_coded_by, eClass_none },
        { CSeqFeatData::eQual_codon, eClass_seq_aa },
        { CSeqFeatData::eQual_codon_start, eClass_int_or },
        { CSeqFeatData::eQual_collected_by, eClass_text },
        { CSeqFeatData::eQual_collection_date, eClass_text },
        { CSeqFeatData::eQual_compare, eClass_text },
        { CSeqFeatData::eQual_cons_splice, eClass_site },
        { CSeqFeatData::eQual_country, eClass_text },
        { CSeqFeatData::eQual_cultivar, eClass_text },
        { CSeqFeatData::eQual_culture_collection, eClass_text },
        { CSeqFeatData::eQual_cyanelle, eClass_none },
        { CSeqFeatData::eQual_db_xref, eClass_text },
        { CSeqFeatData::eQual_derived_from, eClass_none },
        { CSeqFeatData::eQual_dev_stage, eClass_text },
        { CSeqFeatData::eQual_direction, eClass_L_R_B },
        { CSeqFeatData::eQual_EC_number, eClass_ecnum },
        { CSeqFeatData::eQual_ecotype, eClass_text },
        { CSeqFeatData::eQual_environmental_sample, eClass_none },
        { CSeqFeatData::eQual_estimated_length, eClass_text },
        { CSeqFeatData::eQual_evidence, eClass_exper },
        { CSeqFeatData::eQual_exception, eClass_text },
        { CSeqFeatData::eQual_experiment, eClass_text },
        { CSeqFeatData::eQual_focus, eClass_none },
        { CSeqFeatData::eQual_frequency, eClass_text },
        { CSeqFeatData::eQual_function, eClass_text },
        { CSeqFeatData::eQual_gap_type, eClass_text },
        { CSeqFeatData::eQual_gdb_xref, eClass_text },
        { CSeqFeatData::eQual_gene, eClass_text },
        { CSeqFeatData::eQual_gene_synonym, eClass_text },
        { CSeqFeatData::eQual_germline, eClass_none },
        { CSeqFeatData::eQual_haplogroup, eClass_text },
        { CSeqFeatData::eQual_haplotype, eClass_text },
        { CSeqFeatData::eQual_heterogen, eClass_none },
        { CSeqFeatData::eQual_host, eClass_text },
        { CSeqFeatData::eQual_identified_by, eClass_text },
        { CSeqFeatData::eQual_inference, eClass_text },
        { CSeqFeatData::eQual_insertion_seq, eClass_text },
        { CSeqFeatData::eQual_isolate, eClass_text },
        { CSeqFeatData::eQual_isolation_source, eClass_text },
        { CSeqFeatData::eQual_kinetoplast, eClass_none },
        { CSeqFeatData::eQual_lab_host, eClass_text },
        { CSeqFeatData::eQual_label, eClass_token },
        { CSeqFeatData::eQual_lat_lon, eClass_text },
        { CSeqFeatData::eQual_linkage_evidence, eClass_text },
        { CSeqFeatData::eQual_linkage_group, eClass_none },
        { CSeqFeatData::eQual_locus_tag, eClass_text },
        { CSeqFeatData::eQual_macronuclear, eClass_none },
        { CSeqFeatData::eQual_map, eClass_text },
        { CSeqFeatData::eQual_mating_type, eClass_text },
        { CSeqFeatData::eQual_metagenome_source, eClass_text },
        { CSeqFeatData::eQual_metagenomic, eClass_none },
        { CSeqFeatData::eQual_mitochondrion, eClass_none },
        { CSeqFeatData::eQual_mobile_element, eClass_text },
        { CSeqFeatData::eQual_mobile_element_type, eClass_text },
        { CSeqFeatData::eQual_mod_base, eClass_token },
        { CSeqFeatData::eQual_mol_type, eClass_text },
        { CSeqFeatData::eQual_name, eClass_none },
        { CSeqFeatData::eQual_nomenclature, eClass_none },
        { CSeqFeatData::eQual_ncRNA_class, eClass_text },
        { CSeqFeatData::eQual_note, eClass_note },
        { CSeqFeatData::eQual_number, eClass_number },
        { CSeqFeatData::eQual_old_locus_tag, eClass_text },
        { CSeqFeatData::eQual_operon, eClass_text },
        { CSeqFeatData::eQual_organelle, eClass_text },
        { CSeqFeatData::eQual_organism, eClass_text },
        { CSeqFeatData::eQual_partial, eClass_none },
        { CSeqFeatData::eQual_PCR_conditions, eClass_text },
        { CSeqFeatData::eQual_PCR_primers, eClass_text },
        { CSeqFeatData::eQual_phenotype, eClass_text },
        { CSeqFeatData::eQual_plasmid, eClass_text },
        { CSeqFeatData::eQual_pop_variant, eClass_text },
        { CSeqFeatData::eQual_product, eClass_text },
        { CSeqFeatData::eQual_protein_id, eClass_text },
        { CSeqFeatData::eQual_proviral, eClass_none },
        { CSeqFeatData::eQual_pseudo, eClass_none },
        { CSeqFeatData::eQual_pseudogene, eClass_text },
        { CSeqFeatData::eQual_rearranged, eClass_none },
        { CSeqFeatData::eQual_recombination_class, eClass_text },
        { CSeqFeatData::eQual_region_name, eClass_none },
        { CSeqFeatData::eQual_regulatory_class, eClass_text },
        { CSeqFeatData::eQual_replace, eClass_text },
        { CSeqFeatData::eQual_ribosomal_slippage, eClass_none },
        { CSeqFeatData::eQual_rpt_family, eClass_text },
        { CSeqFeatData::eQual_rpt_type, eClass_rpt },
        { CSeqFeatData::eQual_rpt_unit, eClass_text },
        { CSeqFeatData::eQual_rpt_unit_range, eClass_text },
        { CSeqFeatData::eQual_rpt_unit_seq, eClass_text },
        { CSeqFeatData::eQual_satellite, eClass_text },
        { CSeqFeatData::eQual_sec_str_type, eClass_none },
        { CSeqFeatData::eQual_segment, eClass_text },
        { CSeqFeatData::eQual_sequenced_mol, eClass_text },
        { CSeqFeatData::eQual_serotype, eClass_text },
        { CSeqFeatData::eQual_serovar, eClass_text },
        { CSeqFeatData::eQual_sex, eClass_text },
        { CSeqFeatData::eQual_site_type, eClass_none },
        { CSeqFeatData::eQual_specimen_voucher, eClass_text },
        { CSeqFeatData::eQual_standard_name, eClass_text },
        { CSeqFeatData::eQual_strain, eClass_text },
        { CSeqFeatData::eQual_sub_clone, eClass_text },
        { CSeqFeatData::eQual_sub_species, eClass_text },
        { CSeqFeatData::eQual_sub_strain, eClass_text },
        { CSeqFeatData::eQual_submitter_seqid, eClass_text },
        { CSeqFeatData::eQual_tag_peptide, eClass_text },
        { CSeqFeatData::eQual_tissue_lib, eClass_text },
        { CSeqFeatData::eQual_tissue_type, eClass_text },
        { CSeqFeatData::eQual_trans_splicing, eClass_none },
        { CSeqFeatData::eQual_transcript_id, eClass_text },
        { CSeqFeatData::eQual_transgenic, eClass_none },
        { CSeqFeatData::eQual_translation, eClass_text },
        { CSeqFeatData::eQual_transl_except, eClass_pos_aa },
        { CSeqFeatData::eQual_transl_table, eClass_int },
        { CSeqFeatData::eQual_transposon, eClass_text },
        { CSeqFeatData::eQual_type_material, eClass_text },
        { CSeqFeatData::eQual_UniProtKB_evidence, eClass_text },
        { CSeqFeatData::eQual_usedin, eClass_token },
        { CSeqFeatData::eQual_variety, eClass_text },
        { CSeqFeatData::eQual_virion, eClass_none },
        { CSeqFeatData::eQual_whole_replicon, eClass_none },
    };


    auto val_class = QUAL_TYPE_TO_VAL_CLASS_MAP.find(qual_type);
    if (val_class == QUAL_TYPE_TO_VAL_CLASS_MAP.end()) {
        return eClass_unknown;
    }

    return val_class->second;
}

const Char* ParFlat_IntOrString[] = { "1", "2", "3" };

const Char* ParFlat_LRBString[] = { "LEFT", "RIGHT", "BOTH" };

const Char* ParFlat_ExpString[] = {
    "EXPERIMENTAL", "NOT_EXPERIMENTAL"
};

const Char* ParFlat_RptString[] = {
    "tandem",
    "inverted",
    "flanking",
    "terminal",
    "direct",
    "dispersed",
    "nested",
    "long_terminal_repeat",
    "non_LTR_retrotransposon_polymeric_tract",
    "X_element_combinatorial_repeat",
    "Y_prime_element",
    "telomeric_repeat",
    "centromeric_repeat",
    "other"
};


/*-------------------------- GBQualSemanticValid() ------------------------*/
/***************************************************************************
 *  GBQualSemanticValid:
 *  -- returns GB_ERR level, outputs error messages if
 *      'error_msgs', set
 *
 *  -- routine also drop out any unknown qualifier, if
 *      'perform_corrections' is set  10-11-93
 *
 ****************************************************************************/
static int GBQualSemanticValid(TQualVector& quals, bool error_msgs, bool perform_corrections)
{
    int retval = GB_FEAT_ERR_NONE,
        ret    = 0;

    for (TQualVector::iterator cur = quals.begin(); cur != quals.end();) {
        const string& qual_str = (*cur)->GetQual();
        if (qual_str == "gsdb_id") {
            ++cur;
            continue;
        }

        string qs = qual_str;
        if (qs == "geo_loc_name") {
            qs = "country";
        }
        CSeqFeatData::EQualifier qual_type = CSeqFeatData::GetQualifierType(qs);
        if (qual_type == CSeqFeatData::eQual_bad) {
            if (retval < GB_FEAT_ERR_REPAIRABLE) {
                retval = GB_FEAT_ERR_REPAIRABLE;
            }
            if (error_msgs) {
                FtaErrPost(SEV_ERROR, ERR_QUALIFIER_UnknownSpelling, qs);
            }
            if (perform_corrections) {
                cur = quals.erase(cur);
            } else
                ++cur;
        } else {
            switch (GetQualifierClass(qual_type)) {
            case eClass_pos_aa:
                ret = CkQualPosaa(*(*cur), error_msgs);
                if (ret > retval) {
                    retval = ret;
                }
                break;
            case eClass_note:
                ret = CkQualNote(*(*cur), error_msgs, perform_corrections);
                if (ret > retval) {
                    retval = ret;
                }
                break;
            case eClass_text:
                ret = CkQualText(*(*cur), nullptr, false, error_msgs, perform_corrections);
                if (ret > retval) {
                    retval = ret;
                }
                break;
            case eClass_bracket_int:
                ret = CkQualTokenType(*(*cur), error_msgs, ParFlat_BracketInt_type);
                if (ret > retval) {
                    retval = ret;
                }
                break;
            case eClass_seq_aa:
                ret = CkQualSeqaa(*(*cur), error_msgs);
                if (ret > retval) {
                    retval = ret;
                }
                break;
            case eClass_int_or:
                ret = CkQualMatchToken(*(*cur), error_msgs, ParFlat_IntOrString, sizeof(ParFlat_IntOrString) / sizeof(ParFlat_IntOrString[0]));
                if (ret > retval) {
                    retval = ret;
                }
                break;
            case eClass_site:
                ret = CkQualSite(*(*cur), error_msgs);
                if (ret > retval) {
                    retval = ret;
                }
                break;
            case eClass_L_R_B:
                ret = CkQualMatchToken(*(*cur), error_msgs, ParFlat_LRBString, sizeof(ParFlat_LRBString) / sizeof(ParFlat_LRBString[0]));
                if (ret > retval) {
                    retval = ret;
                }
                break;
            case eClass_ecnum:
                ret = CkQualEcnum(*(*cur), error_msgs, perform_corrections);
                if (ret > retval) {
                    retval = ret;
                }
                break;
            case eClass_exper:
                ret = CkQualMatchToken(*(*cur), error_msgs, ParFlat_ExpString, sizeof(ParFlat_ExpString) / sizeof(ParFlat_ExpString[0]));
                if (ret > retval) {
                    retval = ret;
                }
                break;
            case eClass_token:
                ret = CkQualTokenType(*(*cur), error_msgs, ParFlat_Stoken_type);
                if (ret > retval) {
                    retval = ret;
                }
                break;
            case eClass_int:
                ret = CkQualTokenType(*(*cur), error_msgs, ParFlat_Integer_type);
                if (ret > retval) {
                    retval = ret;
                }
                break;
            case eClass_number:
                ret = CkQualTokenType(*(*cur), error_msgs, ParFlat_Number_type);
                if (ret > retval) {
                    retval = ret;
                }
                break;
            case eClass_rpt:
                ret = CkQualMatchToken(*(*cur), error_msgs, ParFlat_RptString, sizeof(ParFlat_RptString) / sizeof(ParFlat_RptString[0]));
                if (ret > retval) {
                    retval = ret;
                }
                break;
            case eClass_flabel_base:
                ret = CkQualTokenType(*(*cur), error_msgs, ParFlat_Stoken_type);
                if (ret > retval) {
                    retval = ret;
                }
                break;
            case eClass_flabel_dbname:
                ret = CkQualTokenType(*(*cur), error_msgs, ParFlat_Stoken_type);
                if (ret > retval) {
                    retval = ret;
                }
                break;
            case eClass_none:
                if ((*cur)->IsSetVal() && ! (*cur)->GetVal().empty()) {
                    if (error_msgs) {
                        FtaErrPost(SEV_ERROR, ERR_QUALIFIER_Xtratext, "/{}={}", qual_str, (*cur)->GetVal());
                    } 
                    retval = GB_FEAT_ERR_REPAIRABLE;
                    if (perform_corrections) {
                        (*cur)->ResetVal();
                    }
                }
            default:
                break;
            } /* switch */

            if (ret == GB_FEAT_ERR_DROP && perform_corrections)
                cur = quals.erase(cur);
            else
                ++cur;
        } /* check qual's value */
    }

    return retval;

} /* GBQualSemanticValid */


/*------------------------------ CkQualPosSeqaa() -------------------------*/
/***************************************************************************
 *  CkQualPosSeqaa:  (called by CkQaulPosaa and ChQualSeqaa)
 *
 *  -- format       (...aa:amino_acid)
 *  -- example     aa:Phe)
 *                                          -Karl 1/28/94
 ****************************************************************************/

static int CkQualPosSeqaa(CGb_qual& cur, bool error_msgs, string& aa, const Char* eptr)
{
    const Char* str;
    int         retval = GB_FEAT_ERR_NONE;

    NStr::TruncateSpacesInPlace(aa, NStr::eTrunc_End);

    string caa = aa;

    size_t comma = caa.find(',');
    if (comma != string::npos) {
        caa = caa.substr(0, comma);
    }

    if (aa == "OTHER" || CCleanup::ValidAminoAcid(caa) != 'X') {
        str = eptr;

        while (*str != '\0' && (*str == ' ' || *str == ')'))
            str++;

        if (*str == '\0') {
            return retval; /* successful, format ok return */
        } else {
            if (error_msgs) {
                FtaErrPost(SEV_ERROR, ERR_QUALIFIER_AA, "Extra text after end /{}={}", cur.GetQual(), cur.GetVal());
            }
            retval = GB_FEAT_ERR_DROP;
        }
    } else {
        if (error_msgs) {
            FtaErrPost(SEV_ERROR, ERR_QUALIFIER_AA, "Bad aa abbreviation<{}>, /{}={}", aa, cur.GetQual(), cur.GetVal());
        }
        retval = GB_FEAT_ERR_DROP;
    }

    return retval;
}


/*------------------------------ CkQualPosaa() -------------------------*/
/***************************************************************************
 *  CkQualPosaa:
 *
 *  -- format       (pos:base_range, aa:amino_acid)
 *  -- example      /anticodon=(pos:34..36,aa:Phe)
 *                  /anticodon=(pos: 34..36, aa: Phe)
 *                                                                 10-12-93
 ****************************************************************************/
static int CkQualPosaa(CGb_qual& cur, bool error_msgs)
{
    const Char* eptr;
    int         retval = GB_FEAT_ERR_NONE;

    const Char* str = cur.GetVal().c_str();

    if (StringEquNI(str, "(pos:", 5)) {
        str += 5;

        while (*str == ' ')
            ++str;

        /*---I expect that we maight need to allow blanks here,
                    but not now... -Karl 1/28/94 */
        if ((eptr = StringChr(str, ','))) {
            while (str != eptr && (isdigit(*str) || *str == '.'))
                str++;

            if (str == eptr) {
                while (*str != '\0' && (*str == ',' || *str == ' '))
                    str++;

                if (StringEquNI(str, "aa:", 3)) {
                    str += 3;

                    while (*str == ' ')
                        ++str;

                    if ((eptr = StringChr(str, ')'))) {
                        string aa(str, eptr);
                        retval = CkQualPosSeqaa(cur, error_msgs, aa, eptr);
                    }
                } /* if, aa: */ else {
                    if (error_msgs) {
                        FtaErrPost(SEV_ERROR, ERR_QUALIFIER_AA, "Missing aa: /{}={}", cur.GetQual(), cur.GetVal());
                    }
                    retval = GB_FEAT_ERR_DROP;
                }
            }
        } else {
            if (error_msgs) {
                FtaErrPost(SEV_ERROR, ERR_QUALIFIER_SeqPosComma, "Missing \',\' /{}={}", cur.GetQual(), cur.GetVal());
                /* ) match */
            }
            retval = GB_FEAT_ERR_DROP;
        }
    } /* if, (pos: */ else {
        if (error_msgs) {
            FtaErrPost(SEV_ERROR, ERR_QUALIFIER_Pos, "Missing (pos: /{}={}", cur.GetQual(), cur.GetVal());
            /* ) match */
        }
        retval = GB_FEAT_ERR_DROP;
    }

    return retval;

} /* CkQualPosaa */

/*------------------------------ CkQualNote() --------------------------*/
/***************************************************************************
*  CkQualNote:
*  -- example: testfile  gbp63.seq gbp88.seq, gbp76.seq
*     /bound_moiety="repressor"
*     /note="Dinucleotide repeat, polymorphic among these rat
      strains:LOU/N>F344/N=BUF/N=MNR/N=WBB1/N=WBB2/N=MR/N=LER/N=ACI/N=SR/Jr=
      SHR/N=WKY/N>BN/SsN=LEW/N (the size of the allelesindicated)."
*     /note="guanine nucleotide-binding protein /hgml-locus_uid='LJ0088P'"
*     /note=" /map='6p21.3' /hgml_locus_uid='LU0011B'"
*
*  -- embedded qualifer
*     -- convert all double quotes to single qutoes
*        (this is unnecessary for the flat2asn parser program, because
*        it only grep first close double quote when "ParseQualifiers" routine
*        build GBQualPtr link list, but it would have post out message if
*        any data was truncated) (I add the conversion because someone may
*        use the routine which parsing the string different from the way I did)
*     -- convert the '/' characters at the start of the embedded-qualifier
*        token to '_'
*                                                                 12-20-93
****************************************************************************/
static int CkQualNote(CGb_qual& cur, bool error_msgs, bool perform_corrections)
{

    bool has_embedded = false;
    int  retval;

    retval = CkQualText(cur, &has_embedded, true, error_msgs, perform_corrections);

    if (has_embedded) {

        string val_str = cur.GetVal();
        std::replace(val_str.begin(), val_str.end(), '\"', '\'');
        cur.SetVal(val_str);
    }

    return retval;

} /* CkQualNote */

/*----------------------- ScanEmbedQual() -----------------------------*/
/****************************************************************************
 *  ScanEmbedQual:
 *  -- retun NULL if no embedded qualifiers found; otherwise, return the
 *     embedded qualifier.
 *  -- scan embedded valid qualifier
 *                                                                  6-29-93
 *****************************************************************************/
static bool ScanEmbedQual(const Char* value)
{
    const Char* bptr;
    const Char* ptr;

    if (value) {
        for (bptr = value; *bptr != '\0';) {
            for (; *bptr != '/' && *bptr != '\0'; bptr++)
                continue;

            if (*bptr == '/') {
                for (++bptr, ptr = bptr; *bptr != '=' && *bptr != ' ' && *bptr != '\0'; bptr++)
                    continue;

                string qual(ptr, bptr);
                if (CSeqFeatData::GetQualifierType(qual) != CSeqFeatData::eQual_bad)
                    return true;
            }
        } /* for */
    }

    return false;

} /* ScanEmbedQual */

/*------------------------------ CkQualText() -------------------------*/
/***************************************************************************
 *  CkQualText:
 *  -- return error severity
 *  -- also check if embedded qualifier
 *  -- format      "text"
 *  if called from /note, ="" will cause qualifier to be dropped.
 *  all others no error, all other, if no qualifier, will add "" value
 ****************************************************************************/
static int CkQualText(CGb_qual& cur,
                      bool*     has_embedded,
                      bool      from_note,
                      bool      error_msgs,
                      bool      perform_corrections)
{
    const Char* bptr;
    const Char* eptr;

    int retval = GB_FEAT_ERR_NONE;

    if (has_embedded) {
        *has_embedded = false;
    }
    if (! cur.IsSetVal()) {
        if (from_note) {
            if (error_msgs) {
                FtaErrPost(SEV_ERROR, ERR_QUALIFIER_EmptyNote, "/note with no text ");
            }
            return GB_FEAT_ERR_DROP;
        } else {
            retval = GB_FEAT_ERR_SILENT;
            if (perform_corrections) {
                cur.SetVal("\"\""); /* yup, a "" string is legal */
            } else {
                return retval;
            }
        }
    }

    const Char* str = cur.GetVal().c_str();
    while (*str != '\0' && (*str == ' ' || *str == '\"')) {
        /* open double quote */
        str++;
        if (*(str - 1) == '\"') {
            break; /* so does not continue through a "" string */
        }
    }
    /* find first close double quote */
    for (bptr = str; *str != '\0' && *str != '\"'; str++)
        continue;
    eptr = str;

    while (*str != '\0' && (*str == ' ' || *str == '\"'))
        str++;

    if (*str != '\0') {
        /*   extra stuff is already rm in ParseQualifiers(). Tatiana*/
        /* extra stuff, if perform corrections, remove it */
        /* ERROR  here  sets retval*/
    }

    string value(bptr, eptr);
    /* Some check must be done for illegal characters in gbpq->val
    for (s = value; *s != '\0'; s++) {
        if (!IS_WHITESP(*s) && !IS_ALPHANUM(*s) && *s != '\"') {
            if (error_msgs){
                FtaErrPost(SEV_WARNING, ERR_QUALIFIER_IllegalCharacter,
                "illegal char [{}] used in qualifier {}", s, gbqp ->qual);
            }
            return (retval > GB_FEAT_ERR_REPAIRABLE) ? retval : GB_FEAT_ERR_REPAIRABLE;
        }
    }
    */

    return retval;
} /* CkQualText */

/*------------------------- CkQualSeqaa() --------------------------*/
/***************************************************************************
 *  CkQualSeqaa:
 *  -- format       (seq:"codon-sequence", aa:amino_acid)
 *  -- example      /codon=(seq:"ttt",aa:Leu)
 *                  /codon=(seq: "ttt", aa: Leu )
 *                                                                  6-29-93
 ***************************************************************************/
static int CkQualSeqaa(CGb_qual& cur, bool error_msgs)
{
    int retval = GB_FEAT_ERR_NONE;

    const Char* str  = cur.GetVal().c_str();
    const Char* eptr = nullptr;

    if (StringEquNI(str, "(seq:", 5)) {
        str += 5;

        while (*str == ' ')
            ++str;

        if ((eptr = StringChr(str, ','))) {
            while (str != eptr)
                str++;

            while (*str != '\0' && (*str == ',' || *str == ' '))
                str++;

            if (StringEquNI(str, "aa:", 3)) {
                str += 3;

                while (*str == ' ')
                    ++str;

                if ((eptr = StringChr(str, ')'))) {
                    string aa(str, eptr);
                    retval = CkQualPosSeqaa(cur, error_msgs, aa, eptr);
                }
            } /* if, aa: */ else {
                if (error_msgs) {
                    FtaErrPost(SEV_ERROR, ERR_QUALIFIER_AA, "Missing aa: /{}={}", cur.GetQual(), cur.GetVal());
                }
                retval = GB_FEAT_ERR_DROP;
            }
        } else {
            if (error_msgs) {
                FtaErrPost(SEV_ERROR, ERR_QUALIFIER_SeqPosComma, "Missing \',\' /{}={}", cur.GetQual(), cur.GetVal());
                /* ) match */
            }
            retval = GB_FEAT_ERR_DROP;
        }
    } /* if, (seq: */ else {


        if (error_msgs) {
            FtaErrPost(SEV_ERROR, ERR_QUALIFIER_Seq, "Missing (seq: /{}={}", cur.GetQual(), cur.GetVal());
            /* ) match */
        }
        retval = GB_FEAT_ERR_DROP;
    }

    return retval;

} /* CkQualSeqaa */

/*------------------------- () -------------------------*/
/*****************************************************************************
 *  CkQualMatchToken:
 *                                                                6-29-93
 *****************************************************************************/
static int CkQualMatchToken(CGb_qual& cur, bool error_msgs, const Char* array_string[], Int2 totalstr)
{
    const Char* bptr;
    const Char* eptr;
    const Char* str;

    int retval = GB_FEAT_ERR_NONE;

    if (! cur.IsSetVal()) {
        if (error_msgs) {
            FtaErrPost(SEV_ERROR, ERR_QUALIFIER_InvalidDataFormat, "NULL value for ({})", cur.GetQual());
        }
        return GB_FEAT_ERR_DROP;
    }
    str = cur.GetVal().c_str();

    for (bptr = str; *str != '\0' && *str != ' '; str++)
        continue;
    eptr = str;

    while (*str != '\0' && *str == ' ')
        str++;

    if (*str == '\0') {
        string msg(bptr, eptr);
        bool   found = false;
        for (Int2 i = 0; i < totalstr; ++i) {
            if (NStr::EqualNocase(msg, array_string[i])) {
                found = true;
                break;
            }
        }

        if (! found) {
            if (error_msgs) {
                FtaErrPost(SEV_ERROR, ERR_QUALIFIER_InvalidDataFormat, "Value not in list of legal values /{}={}", cur.GetQual(), cur.GetVal());
            }
            retval = GB_FEAT_ERR_DROP;
        }
    } else {
        if (error_msgs) {
            FtaErrPost(SEV_ERROR, ERR_QUALIFIER_Too_many_tokens, "/{}={}", cur.GetQual(), cur.GetVal());
        }
        retval = GB_FEAT_ERR_DROP;
    }

    return retval;

} /* CkQualMatchToken */

/*------------------------- CkQualEcnum() ---------------------------*/
/***************************************************************************
 *   CkQualEcnum:
 *   -- Ec_num has text format,
 *      but the text only allow digits, period, and hyphen (-)
 *                                                                12-10-93
 ****************************************************************************/
static int CkQualEcnum(CGb_qual& cur, bool error_msgs, bool perform_corrections)
{
    const Char* str;
    int         retval = GB_FEAT_ERR_NONE;


    retval = CkQualText(cur, nullptr, false, error_msgs, perform_corrections);
    if (retval == GB_FEAT_ERR_NONE) {

        str = cur.GetVal().c_str();
        /* open double quote */
        while (*str != '\0' && (*str == ' ' || *str == '\"'))
            str++;

        for (; *str != '\0' && *str != '\"'; str++)
            if (! isdigit(*str) && *str != '.' && *str != '-' && *str != 'n') {
                if (error_msgs) {
                    FtaErrPost(SEV_ERROR, ERR_QUALIFIER_BadECnum, "At <{}>({}) /{}={}", *str, (int)*str, cur.GetQual(), cur.GetVal());
                }
                retval = GB_FEAT_ERR_DROP;
                break;
            }
    }

    return retval;

} /* CkQualEcnum */

/*------------------------- CkQualSite() --------------------------*/
/***************************************************************************
 *  CkQualSite:
 *  -- format       (5'site:bool, 3'site:bool)
 *  -- example      /cons_splice=(5'site:YES, 3'site:NO)
 *                                                                  6-29-93
 ***************************************************************************/
static int CkQualSite(CGb_qual& cur, bool error_msgs)
{
    int         retval = GB_FEAT_ERR_NONE;
    const Char* bptr;
    const Char* str;

    bool        ok        = false;
    const Char* yes_or_no = "not \'YES\', \'NO\' or \'ABSENT\'";

    str = cur.GetVal().c_str();
    if (StringEquNI(str, "(5'site:", 8)) {
        bptr = str;
        str += 8;

        if (StringEquNI(str, "YES", 3) || StringEquNI(str, "NO", 2) ||
            StringEquNI(str, "ABSENT", 6)) {

            if (StringEquNI(str, "YES", 3))
                str += 3;
            else if (StringEquNI(str, "NO", 2))
                str += 2;
            else
                str += 6;

            for (; *str == ' '; str++)
                ;
            for (; *str == ','; str++)
                ;
            for (; *str == ' '; str++)
                ;


            if (StringEquNI(str, "3'site:", 7)) {
                str += 7;

                if (StringEquNI(str, "YES", 3) ||
                    StringEquNI(str, "NO", 2) ||
                    StringEquNI(str, "ABSENT", 6)) {
                    if (StringEquNI(str, "YES", 3))
                        str += 3;
                    else if (StringEquNI(str, "NO", 2))
                        str += 2;
                    else
                        str += 6;

                    if (*str == ')') {

                        while (*str != '\0' && (*str == ' ' || *str == ')'))
                            str++;

                        if (*str == '\0')
                            ok = true;
                        else {
                            bptr = "extra characters";
                        }

                    } /* if, ")" */ else {
                    }
                } /* if, yes or no */ else {
                    bptr = yes_or_no;
                }
            } /* if, 3'site */ else {
                bptr = "3\' site";
            }
        } /* if, yes or no */ else {
            bptr = yes_or_no;
        }
    } /* if, 5'site */ else {
        bptr = "5\' site";
    }

    if (! ok) {
        if (error_msgs) {
            FtaErrPost(SEV_ERROR, ERR_QUALIFIER_Cons_splice, "{} /{}={}", bptr, cur.GetQual(), cur.GetVal());
        }
        retval = GB_FEAT_ERR_DROP;
    }
    return retval;

} /* CkQualSite */

/*------------------------- CkQualTokenType() --------------------------*/
/***************************************************************************
 *  CkQualTokenType:
 *  -- format   single token
 *  -- example  ParFlat_Stoken_type        /label=Albl_exonl  /mod_base=m5c
 *              ParFlat_BracketInt_type    /citation=[3] or /citation= ([1],[3])
 *              ParFlat_Integer_type           /transl_table=4
 *              ParFlat_Number_type        /number=4b
 *  -- not implemented yet, treat as ParFlat_Stoken_type:
 *     -- feature_label or base_range
 *                 /rpt_unit=Alu_rpt1   /rpt_unit=202..245
 *     -- Accession-number:feature-name or
 *                            Database_name:Acc_number:feature_label
 *        /usedin=X10087:proteinx
 *                                                                 10-12-93
 ***************************************************************************/
static int CkQualTokenType(CGb_qual& cur, bool error_msgs, Uint1 type)
{
    const Char* bptr;
    const Char* eptr;
    const Char* str;

    bool token_there = false;
    int  retval      = GB_FEAT_ERR_NONE;

    str = cur.IsSetVal() ? cur.GetVal().c_str() : nullptr;

    if (str)
        if (*str != '\0') {
            token_there = true;
        }
    if (! token_there) {
        if (error_msgs) {
            FtaErrPost(SEV_ERROR, ERR_QUALIFIER_InvalidDataFormat, "Missing value /{}=...", cur.GetQual());
        }
        retval = GB_FEAT_ERR_DROP;
    } else {
        /*  token there */
        for (bptr = str; *str != '\0' && *str != ' '; str++)
            continue;
        eptr = str;

        while (*str != '\0' && *str == ' ')
            str++;

        if (*str == '\0') {
            /*------single token found ----*/
            string token(bptr, eptr);

            switch (type) {
            case ParFlat_BracketInt_type:
                /*-------this can be made to be much more rigorous --Karl ---*/
                str = CkBracketType(token.c_str());
                break;
            case ParFlat_Integer_type:
                for (str = token.c_str(); *str != '\0' && isdigit(*str); str++)
                    continue;
                if (*str == '\0') {
                    str = nullptr;
                }
                break;
            case ParFlat_Stoken_type:
                str = CkLabelType(token.c_str());
                break;
            case ParFlat_Number_type:
                str = CkNumberType(token.c_str());
                break;
            default:
                str = nullptr;
                break;
            }

            if (str) {
#if 0
                switch (type) {
                case ParFlat_BracketInt_type:
                    bptr = "Invalid [integer] format";
                    break;
                case ParFlat_Integer_type:
                    bptr = "Not an integer number";
                    break;
                case ParFlat_Stoken_type:
                    bptr = "Invalid format";
                    break;
                    /*-- logically can not happen, as coded now -Karl  1/31/94 --*/
                default:
                    bptr = "Bad qualifier value";
                    break;
                }
#endif
                if (error_msgs) {
                    FtaErrPost(SEV_ERROR, ERR_QUALIFIER_InvalidDataFormat, "{}={}, at {}", cur.GetQual(), cur.GetVal(), str);
                }
                retval = GB_FEAT_ERR_DROP;
            }
        } else {
            /*-- more than a single token found ---*/
            if (error_msgs) {
                FtaErrPost(SEV_ERROR, ERR_QUALIFIER_Xtratext, "extra text found /{}={}, at {}", cur.GetQual(), cur.GetVal(), str);
            }
            retval = GB_FEAT_ERR_DROP;
        }
    } /* token there */

    return retval;

} /* CkQualTokenType */


/*------------------------ CkBracketType() --------------------*/
/*****************************************************************************
 *   CkBracketType:
 *      checks /citation=([1],[3])
 *     May be we should check for only single value here like
 *      /citation=[digit]
 *                                                          -Tatiana 1/28/95
 ******************************************************************************/
static const Char* CkBracketType(const Char* str)
{
    if (! str)
        return "NULL value";
    if (*str == '[') {
        str++;
        if (! isdigit(*str)) {
            return str;
        } else {
            while (isdigit(*str)) {
                str++;
            }
            if (*str != ']') {
                return str;
            }
            str++;
            if (*str != '\0') {
                return str;
            }
            return nullptr;
        }
    } else {
        return str;
    }
}

/*------------------------ CkNumberType() --------------------*/
/*****************************************************************************
 *   CkNumberType:
 *      checks /number=single_token  - numbers and letters
 *      /number=4 or /number=6b
 *                                                          -Tatiana 2/1/00
 ******************************************************************************/
static const Char* CkNumberType(const Char* str)
{
    for (; *str != '\0' && ! isalnum(*str); str++)
        continue;
    if (*str != '\0') {
        return nullptr;
    }
    return str;
}

/*------------------------ CkLabelType() --------------------*/
/*****************************************************************************
 *   CkLabelType:
 *      checks /label=,feature_label> or /label=<base_range>
 *                                                          -Tatiana 1/28/95
 ******************************************************************************/
static const Char* CkLabelType(const Char* str)
{
    bool        range = true, label = true;
    const Char* bptr;

    if (isdigit(*str)) {
        for (; isdigit(*str); str++)
            continue;
        if (*str == '.' && *(str + 1) == '.') {
            str += 2;
            if (! isdigit(*str)) {
                range = false;
            } else {
                while (isdigit(*str)) {
                    str++;
                }
            }
            if (*str != '\0') {
                range = false;
            }
        } else {
            range = false;
        }
    }
    if (! range) {
        bptr = str;
        for (; *str != '\0' && ! isalpha(*str); str++)
            continue;
        if (*str == '\0') {
            label = false; /* must be at least one letter */
        }
        for (str = bptr; (*str != '\0' && isalpha(*str)) || isdigit(*str) || *str == '-' || *str == '_' || *str == '\'' || *str == '*';
             str++)
            continue;
        if (*str != '\0') {
            label = false;
        }
    }
    if (range || label) {
        return nullptr;
    } else {
        return str;
    }
}

END_NCBI_SCOPE
