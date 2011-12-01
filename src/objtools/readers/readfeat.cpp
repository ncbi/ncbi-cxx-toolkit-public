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
 * Author:  Jonathan Kans
 *
 * File Description:
 *   Feature table reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>

#include <util/static_map.hpp>

#include <serial/iterator.hpp>
#include <serial/objistrasn.hpp>

// Objects includes
#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seq/seq_loc_from_string.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objtools/readers/readfeat.hpp>
#include <objtools/readers/table_filter.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>

#include <objtools/readers/error_container.hpp>

#include "best_feat_finder.hpp"

#define NCBI_USE_ERRCODE_X   Objtools_Rd_Feature

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class /* NCBI_XOBJREAD_EXPORT */ CFeature_table_reader_imp
{
public:
    enum EQual {
        eQual_allele,
        eQual_anticodon,
        eQual_bac_ends,
        eQual_bond_type,
        eQual_bound_moiety,
        eQual_chrcnt,
        eQual_citation,
        eQual_clone,
        eQual_clone_id,
        eQual_codon_recognized,
        eQual_codon_start,
        eQual_compare,
        eQual_cons_splice,
        eQual_ctgcnt,
        eQual_db_xref,
        eQual_direction,
        eQual_EC_number,
        eQual_evidence,
        eQual_exception,
        eQual_experiment,
        eQual_frequency,
        eQual_function,
        eQual_gene,
        eQual_gene_desc,
        eQual_gene_syn,
        eQual_go_component,
        eQual_go_function,
        eQual_go_process,
        eQual_heterogen,
        eQual_inference,
        eQual_insertion_seq,
        eQual_label,
        eQual_loccnt,
        eQual_locus_tag,
        eQual_macronuclear,
        eQual_map,
        eQual_MEDLINE,
        eQual_method,
        eQual_mod_base,
        eQual_muid,
        eQual_ncRNA_class,
        eQual_nomenclature,
        eQual_note,
        eQual_number,
        eQual_old_locus_tag,
        eQual_operon,
        eQual_organism,
        eQual_partial,
        eQual_PCR_conditions,
        eQual_phenotype,
        eQual_pmid,
        eQual_product,
        eQual_prot_desc,
        eQual_prot_note,
        eQual_protein_id,
        eQual_pseudo,
        eQual_PubMed,
        eQual_region_name,
        eQual_replace,
        eQual_ribosomal_slippage,
        eQual_rpt_family,
        eQual_rpt_type,
        eQual_rpt_unit,
        eQual_rpt_unit_range,
        eQual_rpt_unit_seq,
        eQual_satellite,
        eQual_sec_str_type,
        eQual_secondary_accession,
        eQual_sequence,
        eQual_site_type,
        eQual_snp_class,
        eQual_snp_gtype,
        eQual_snp_het,
        eQual_snp_het_se,
        eQual_snp_linkout,
        eQual_snp_maxrate,
        eQual_snp_valid,
        eQual_standard_name,
        eQual_STS,
        eQual_sts_aliases,
        eQual_sts_dsegs,
        eQual_tag_peptide,
        eQual_trans_splicing,
        eQual_transcript_id,
        eQual_transcription,
        eQual_transl_except,
        eQual_transl_table,
        eQual_translation,
        eQual_transposon,
        eQual_usedin,
        eQual_weight
    };

    enum EOrgRef {
        eOrgRef_organism,
        eOrgRef_organelle,
        eOrgRef_div,
        eOrgRef_lineage,
        eOrgRef_gcode,
        eOrgRef_mgcode
    };

    // constructor
    CFeature_table_reader_imp(void);
    // destructor
    ~CFeature_table_reader_imp(void);

    // read 5-column feature table and return Seq-annot
    CRef<CSeq_annot> ReadSequinFeatureTable (ILineReader& reader,
                                             const string& seqid,
                                             const string& annotname,
                                             const CFeature_table_reader::TFlags flags, 
                                             IErrorContainer* container,
                                             ITableFilter *filter);

    // create single feature from key
    CRef<CSeq_feat> CreateSeqFeat (const string& feat,
                                   CSeq_loc& location,
                                   const CFeature_table_reader::TFlags flags, 
                                   IErrorContainer* container,
                                   unsigned int line,
                                   const string &seq_id,
                                   ITableFilter *filter);

    // add single qualifier to feature
    void AddFeatQual (CRef<CSeq_feat> sfp,
                      const string& feat_name,
                      const string& qual,
                      const string& val,
                      const CFeature_table_reader::TFlags flags,
                      IErrorContainer* container,
                      int line,
                      const string &seq_id );

private:

    // Prohibit copy constructor and assignment operator
    CFeature_table_reader_imp(const CFeature_table_reader_imp& value);
    CFeature_table_reader_imp& operator=(const CFeature_table_reader_imp& value);

    bool x_ParseFeatureTableLine (const string& line, Int4* startP, Int4* stopP,
                                  bool* partial5P, bool* partial3P, bool* ispointP, bool* isminusP,
                                  string& featP, string& qualP, string& valP, Int4 offset,
                                  IErrorContainer *container, int line_num, const string &seq_id );

    bool x_AddIntervalToFeature (CRef<CSeq_feat> sfp, CSeq_loc_mix& mix,
                                 const string& seqid, Int4 start, Int4 stop,
                                 bool partial5, bool partial3, bool ispoint, bool isminus);

    bool x_AddQualifierToFeature (CRef<CSeq_feat> sfp,
        const string &feat_name,
        const string& qual, const string& val,
        IErrorContainer *container, int line_num, const string &seq_id );

    bool x_AddQualifierToGene     (CSeqFeatData& sfdata,
                                   EQual qtype, const string& val);
    bool x_AddQualifierToCdregion (CRef<CSeq_feat> sfp, CSeqFeatData& sfdata,
                                   EQual qtype, const string& val,
                                   IErrorContainer *container, int line_num, const string &seq_id );
    bool x_AddQualifierToRna      (CSeqFeatData& sfdata,
                                   EQual qtype, const string& val,
                                   IErrorContainer *container, int line_num, const string &seq_id );
    bool x_AddQualifierToImp      (CRef<CSeq_feat> sfp, CSeqFeatData& sfdata,
                                   EQual qtype, const string& qual, const string& val);
    bool x_AddQualifierToBioSrc   (CSeqFeatData& sfdata,
                                   const string &feat_name,
                                   EOrgRef rtype, const string& val,
                                   IErrorContainer *container, int line, const string &seq_id );
    bool x_AddQualifierToBioSrc   (CSeqFeatData& sfdata,
                                   CSubSource::ESubtype stype, const string& val);
    bool x_AddQualifierToBioSrc   (CSeqFeatData& sfdata,
                                   COrgMod::ESubtype mtype, const string& val);

    bool x_AddGBQualToFeature    (CRef<CSeq_feat> sfp,
                                  const string& qual, const string& val);

    bool x_StringIsJustQuotes (const string& str);

    int x_ParseTrnaString (const string& val);

    void x_ParseTrnaExtString(
        CTrna_ext & ext_trna, const string & str, const CSeq_id *seq_id );
    SIZE_TYPE x_MatchingParenPos( const string &str, SIZE_TYPE open_paren_pos );

    long x_StringToLongNoThrow (
        CTempString strToConvert,
        IErrorContainer *container, 
        const std::string& strSeqId,
        unsigned int uLine,
        CTempString strFeatureName,
        CTempString strQualifierName,
        // user can override the default problem types that are set on error
        ILineError::EProblem eProblem = ILineError::eProblem_Unset
    );

    bool x_SetupSeqFeat (CRef<CSeq_feat> sfp, const string& feat,
                         const CFeature_table_reader::TFlags flags, 
                         unsigned int line,
                         const string &seq_id,
                         IErrorContainer* container,
                         ITableFilter *filter);

    void  x_ProcessMsg (
        IErrorContainer* container,
        ILineError::EProblem eProblem,
        EDiagSev eSeverity,
        const std::string& strSeqId,
        unsigned int uLine,
        const std::string & strFeatureName = string(""),
        const std::string & strQualifierName = string(""),
        const std::string & strQualifierValue = string("")  );

    void x_TokenizeStrict( const string &line, vector<string> &out_tokens );
    void x_TokenizeLenient( const string &line, vector<string> &out_tokens );

};

auto_ptr<CFeature_table_reader_imp> CFeature_table_reader::sm_Implementation;

void CFeature_table_reader::x_InitImplementation()
{
    DEFINE_STATIC_FAST_MUTEX(s_Implementation_mutex);

    CFastMutexGuard   LOCK(s_Implementation_mutex);
    if ( !sm_Implementation.get() ) {
        sm_Implementation.reset(new CFeature_table_reader_imp());
    }
}


typedef pair <const char *, const CSeqFeatData::ESubtype> TFeatKey;

static const TFeatKey feat_key_to_subtype [] = {
    TFeatKey ( "-10_signal",         CSeqFeatData::eSubtype_10_signal          ),
    TFeatKey ( "-35_signal",         CSeqFeatData::eSubtype_35_signal          ),
    TFeatKey ( "3'UTR",              CSeqFeatData::eSubtype_3UTR               ),
    TFeatKey ( "3'clip",             CSeqFeatData::eSubtype_3clip              ),
    TFeatKey ( "5'UTR",              CSeqFeatData::eSubtype_5UTR               ),
    TFeatKey ( "5'clip",             CSeqFeatData::eSubtype_5clip              ),
    TFeatKey ( "Bond",               CSeqFeatData::eSubtype_bond               ),
    TFeatKey ( "CAAT_signal",        CSeqFeatData::eSubtype_CAAT_signal        ),
    TFeatKey ( "CDS",                CSeqFeatData::eSubtype_cdregion           ),
    TFeatKey ( "C_region",           CSeqFeatData::eSubtype_C_region           ),
    TFeatKey ( "Cit",                CSeqFeatData::eSubtype_pub                ),
    TFeatKey ( "CloneRef",           CSeqFeatData::eSubtype_clone              ),
    TFeatKey ( "Comment",            CSeqFeatData::eSubtype_comment            ),
    TFeatKey ( "D-loop",             CSeqFeatData::eSubtype_D_loop             ),
    TFeatKey ( "D_segment",          CSeqFeatData::eSubtype_D_segment          ),
    TFeatKey ( "GC_signal",          CSeqFeatData::eSubtype_GC_signal          ),
    TFeatKey ( "Het",                CSeqFeatData::eSubtype_het                ),
    TFeatKey ( "J_segment",          CSeqFeatData::eSubtype_J_segment          ),
    TFeatKey ( "LTR",                CSeqFeatData::eSubtype_LTR                ),
    TFeatKey ( "N_region",           CSeqFeatData::eSubtype_N_region           ),
    TFeatKey ( "NonStdRes",          CSeqFeatData::eSubtype_non_std_residue    ),
    TFeatKey ( "Num",                CSeqFeatData::eSubtype_num                ),
    TFeatKey ( "Protein",            CSeqFeatData::eSubtype_prot               ),
    TFeatKey ( "RBS",                CSeqFeatData::eSubtype_RBS                ),
    TFeatKey ( "REFERENCE",          CSeqFeatData::eSubtype_pub                ),
    TFeatKey ( "Region",             CSeqFeatData::eSubtype_region             ),
    TFeatKey ( "Rsite",              CSeqFeatData::eSubtype_rsite              ),
    TFeatKey ( "STS",                CSeqFeatData::eSubtype_STS                ),
    TFeatKey ( "S_region",           CSeqFeatData::eSubtype_S_region           ),
    TFeatKey ( "SecStr",             CSeqFeatData::eSubtype_psec_str           ),
    TFeatKey ( "Site",               CSeqFeatData::eSubtype_site               ),
    TFeatKey ( "Site-ref",           CSeqFeatData::eSubtype_site_ref           ),
    TFeatKey ( "Src",                CSeqFeatData::eSubtype_biosrc             ),
    TFeatKey ( "TATA_signal",        CSeqFeatData::eSubtype_TATA_signal        ),
    TFeatKey ( "TxInit",             CSeqFeatData::eSubtype_txinit             ),
    TFeatKey ( "User",               CSeqFeatData::eSubtype_user               ),
    TFeatKey ( "V_region",           CSeqFeatData::eSubtype_V_region           ),
    TFeatKey ( "V_segment",          CSeqFeatData::eSubtype_V_segment          ),
    TFeatKey ( "VariationRef",       CSeqFeatData::eSubtype_variation_ref      ),
    TFeatKey ( "Xref",               CSeqFeatData::eSubtype_seq                ),
    TFeatKey ( "attenuator",         CSeqFeatData::eSubtype_attenuator         ),
    TFeatKey ( "conflict",           CSeqFeatData::eSubtype_conflict           ),
    TFeatKey ( "enhancer",           CSeqFeatData::eSubtype_enhancer           ),
    TFeatKey ( "exon",               CSeqFeatData::eSubtype_exon               ),
    TFeatKey ( "gap",                CSeqFeatData::eSubtype_gap                ),
    TFeatKey ( "gene",               CSeqFeatData::eSubtype_gene               ),
    TFeatKey ( "iDNA",               CSeqFeatData::eSubtype_iDNA               ),
    TFeatKey ( "intron",             CSeqFeatData::eSubtype_intron             ),
    TFeatKey ( "mRNA",               CSeqFeatData::eSubtype_mRNA               ),
    TFeatKey ( "mat_peptide",        CSeqFeatData::eSubtype_mat_peptide_aa     ),
    TFeatKey ( "mat_peptide_nt",     CSeqFeatData::eSubtype_mat_peptide        ),
    TFeatKey ( "misc_RNA",           CSeqFeatData::eSubtype_otherRNA           ),
    TFeatKey ( "misc_binding",       CSeqFeatData::eSubtype_misc_binding       ),
    TFeatKey ( "misc_difference",    CSeqFeatData::eSubtype_misc_difference    ),
    TFeatKey ( "misc_feature",       CSeqFeatData::eSubtype_misc_feature       ),
    TFeatKey ( "misc_recomb",        CSeqFeatData::eSubtype_misc_recomb        ),
    TFeatKey ( "misc_signal",        CSeqFeatData::eSubtype_misc_signal        ),
    TFeatKey ( "misc_structure",     CSeqFeatData::eSubtype_misc_structure     ),
    TFeatKey ( "modified_base",      CSeqFeatData::eSubtype_modified_base      ),
    TFeatKey ( "ncRNA",              CSeqFeatData::eSubtype_ncRNA              ),
    TFeatKey ( "old_sequence",       CSeqFeatData::eSubtype_old_sequence       ),
    TFeatKey ( "operon",             CSeqFeatData::eSubtype_operon             ),
    TFeatKey ( "oriT",               CSeqFeatData::eSubtype_oriT               ),
    TFeatKey ( "polyA_signal",       CSeqFeatData::eSubtype_polyA_signal       ),
    TFeatKey ( "polyA_site",         CSeqFeatData::eSubtype_polyA_site         ),
    TFeatKey ( "pre_RNA",            CSeqFeatData::eSubtype_preRNA             ),
    TFeatKey ( "precursor_RNA",      CSeqFeatData::eSubtype_preRNA             ),
    TFeatKey ( "preprotein",         CSeqFeatData::eSubtype_preprotein         ),
    TFeatKey ( "prim_transcript",    CSeqFeatData::eSubtype_prim_transcript    ),
    TFeatKey ( "primer_bind",        CSeqFeatData::eSubtype_primer_bind        ),
    TFeatKey ( "promoter",           CSeqFeatData::eSubtype_promoter           ),
    TFeatKey ( "protein_bind",       CSeqFeatData::eSubtype_protein_bind       ),
    TFeatKey ( "rRNA",               CSeqFeatData::eSubtype_rRNA               ),
    TFeatKey ( "rep_origin",         CSeqFeatData::eSubtype_rep_origin         ),
    TFeatKey ( "repeat_region",      CSeqFeatData::eSubtype_repeat_region      ),
    TFeatKey ( "repeat_unit",        CSeqFeatData::eSubtype_repeat_unit        ),
    TFeatKey ( "satellite",          CSeqFeatData::eSubtype_satellite          ),
    TFeatKey ( "scRNA",              CSeqFeatData::eSubtype_scRNA              ),
    TFeatKey ( "sig_peptide",        CSeqFeatData::eSubtype_sig_peptide_aa     ),
    TFeatKey ( "sig_peptide_nt",     CSeqFeatData::eSubtype_sig_peptide        ),
    TFeatKey ( "snRNA",              CSeqFeatData::eSubtype_snRNA              ),
    TFeatKey ( "snoRNA",             CSeqFeatData::eSubtype_snoRNA             ),
    TFeatKey ( "source",             CSeqFeatData::eSubtype_biosrc             ),
    TFeatKey ( "stem_loop",          CSeqFeatData::eSubtype_stem_loop          ),
    TFeatKey ( "tRNA",               CSeqFeatData::eSubtype_tRNA               ),
    TFeatKey ( "terminator",         CSeqFeatData::eSubtype_terminator         ),
    TFeatKey ( "tmRNA",              CSeqFeatData::eSubtype_tmRNA              ),
    TFeatKey ( "transit_peptide",    CSeqFeatData::eSubtype_transit_peptide_aa ),
    TFeatKey ( "transit_peptide_nt", CSeqFeatData::eSubtype_transit_peptide    ),
    TFeatKey ( "unsure",             CSeqFeatData::eSubtype_unsure             ),
    TFeatKey ( "variation",          CSeqFeatData::eSubtype_variation          ),
    TFeatKey ( "virion",             CSeqFeatData::eSubtype_virion             )
};

typedef CStaticArrayMap <const char*, const CSeqFeatData::ESubtype, PCase_CStr> TFeatMap;
DEFINE_STATIC_ARRAY_MAP(TFeatMap, sm_FeatKeys, feat_key_to_subtype);


typedef pair <const char *, const CFeature_table_reader_imp::EQual> TQualKey;

static const TQualKey qual_key_to_subtype [] = {
    TQualKey ( "EC_number",            CFeature_table_reader_imp::eQual_EC_number            ),
    TQualKey ( "PCR_conditions",       CFeature_table_reader_imp::eQual_PCR_conditions       ),
    TQualKey ( "PubMed",               CFeature_table_reader_imp::eQual_PubMed               ),
    TQualKey ( "STS",                  CFeature_table_reader_imp::eQual_STS                  ),
    TQualKey ( "allele",               CFeature_table_reader_imp::eQual_allele               ),
    TQualKey ( "anticodon",            CFeature_table_reader_imp::eQual_anticodon            ),
    TQualKey ( "bac_ends",             CFeature_table_reader_imp::eQual_bac_ends             ),
    TQualKey ( "bond_type",            CFeature_table_reader_imp::eQual_bond_type            ),
    TQualKey ( "bound_moiety",         CFeature_table_reader_imp::eQual_bound_moiety         ),
    TQualKey ( "chrcnt",               CFeature_table_reader_imp::eQual_chrcnt               ),
    TQualKey ( "citation",             CFeature_table_reader_imp::eQual_citation             ),
    TQualKey ( "clone",                CFeature_table_reader_imp::eQual_clone                ),
    TQualKey ( "clone_id",             CFeature_table_reader_imp::eQual_clone_id             ),
    TQualKey ( "codon_recognized",     CFeature_table_reader_imp::eQual_codon_recognized     ),
    TQualKey ( "codon_start",          CFeature_table_reader_imp::eQual_codon_start          ),
    TQualKey ( "codons_recognized",    CFeature_table_reader_imp::eQual_codon_recognized     ),
    TQualKey ( "compare",              CFeature_table_reader_imp::eQual_compare              ),
    TQualKey ( "cons_splice",          CFeature_table_reader_imp::eQual_cons_splice          ),
    TQualKey ( "ctgcnt",               CFeature_table_reader_imp::eQual_ctgcnt               ),
    TQualKey ( "db_xref",              CFeature_table_reader_imp::eQual_db_xref              ),
    TQualKey ( "direction",            CFeature_table_reader_imp::eQual_direction            ),
    TQualKey ( "evidence",             CFeature_table_reader_imp::eQual_evidence             ),
    TQualKey ( "exception",            CFeature_table_reader_imp::eQual_exception            ),
    TQualKey ( "experiment",           CFeature_table_reader_imp::eQual_experiment           ),
    TQualKey ( "frequency",            CFeature_table_reader_imp::eQual_frequency            ),
    TQualKey ( "function",             CFeature_table_reader_imp::eQual_function             ),
    TQualKey ( "gene",                 CFeature_table_reader_imp::eQual_gene                 ),
    TQualKey ( "gene_desc",            CFeature_table_reader_imp::eQual_gene_desc            ),
    TQualKey ( "gene_syn",             CFeature_table_reader_imp::eQual_gene_syn             ),
    TQualKey ( "gene_synonym",         CFeature_table_reader_imp::eQual_gene_syn             ),
    TQualKey ( "go_component",         CFeature_table_reader_imp::eQual_go_component         ),
    TQualKey ( "go_function",          CFeature_table_reader_imp::eQual_go_function          ),
    TQualKey ( "go_process",           CFeature_table_reader_imp::eQual_go_process           ),
    TQualKey ( "heterogen",            CFeature_table_reader_imp::eQual_heterogen            ),
    TQualKey ( "inference",            CFeature_table_reader_imp::eQual_inference            ),
    TQualKey ( "insertion_seq",        CFeature_table_reader_imp::eQual_insertion_seq        ),
    TQualKey ( "label",                CFeature_table_reader_imp::eQual_label                ),
    TQualKey ( "loccnt",               CFeature_table_reader_imp::eQual_loccnt               ),
    TQualKey ( "locus_tag",            CFeature_table_reader_imp::eQual_locus_tag            ),
    TQualKey ( "macronuclear",         CFeature_table_reader_imp::eQual_macronuclear         ),
    TQualKey ( "map",                  CFeature_table_reader_imp::eQual_map                  ),
    TQualKey ( "method",               CFeature_table_reader_imp::eQual_method               ),
    TQualKey ( "mod_base",             CFeature_table_reader_imp::eQual_mod_base             ),
    TQualKey ( "ncRNA_class",          CFeature_table_reader_imp::eQual_ncRNA_class          ),
    TQualKey ( "nomenclature",         CFeature_table_reader_imp::eQual_nomenclature         ),
    TQualKey ( "note",                 CFeature_table_reader_imp::eQual_note                 ),
    TQualKey ( "number",               CFeature_table_reader_imp::eQual_number               ),
    TQualKey ( "old_locus_tag",        CFeature_table_reader_imp::eQual_old_locus_tag        ),
    TQualKey ( "operon",               CFeature_table_reader_imp::eQual_operon               ),
    TQualKey ( "organism",             CFeature_table_reader_imp::eQual_organism             ),
    TQualKey ( "partial",              CFeature_table_reader_imp::eQual_partial              ),
    TQualKey ( "phenotype",            CFeature_table_reader_imp::eQual_phenotype            ),
    TQualKey ( "product",              CFeature_table_reader_imp::eQual_product              ),
    TQualKey ( "prot_desc",            CFeature_table_reader_imp::eQual_prot_desc            ),
    TQualKey ( "prot_note",            CFeature_table_reader_imp::eQual_prot_note            ),
    TQualKey ( "protein_id",           CFeature_table_reader_imp::eQual_protein_id           ),
    TQualKey ( "pseudo",               CFeature_table_reader_imp::eQual_pseudo               ),
    TQualKey ( "replace",              CFeature_table_reader_imp::eQual_replace              ),
    TQualKey ( "ribosomal_slippage",   CFeature_table_reader_imp::eQual_ribosomal_slippage   ),
    TQualKey ( "rpt_family",           CFeature_table_reader_imp::eQual_rpt_family           ),
    TQualKey ( "rpt_type",             CFeature_table_reader_imp::eQual_rpt_type             ),
    TQualKey ( "rpt_unit",             CFeature_table_reader_imp::eQual_rpt_unit             ),
    TQualKey ( "rpt_unit_range",       CFeature_table_reader_imp::eQual_rpt_unit_range       ),
    TQualKey ( "rpt_unit_seq",         CFeature_table_reader_imp::eQual_rpt_unit_seq         ),
    TQualKey ( "satellite",            CFeature_table_reader_imp::eQual_satellite            ),
    TQualKey ( "sec_str_type",         CFeature_table_reader_imp::eQual_sec_str_type         ),
    TQualKey ( "secondary_accession",  CFeature_table_reader_imp::eQual_secondary_accession  ),
    TQualKey ( "secondary_accessions", CFeature_table_reader_imp::eQual_secondary_accession  ),
    TQualKey ( "sequence",             CFeature_table_reader_imp::eQual_sequence             ),
    TQualKey ( "site_type",            CFeature_table_reader_imp::eQual_site_type            ),
    TQualKey ( "snp_class",            CFeature_table_reader_imp::eQual_snp_class            ),
    TQualKey ( "snp_gtype",            CFeature_table_reader_imp::eQual_snp_gtype            ),
    TQualKey ( "snp_het",              CFeature_table_reader_imp::eQual_snp_het              ),
    TQualKey ( "snp_het_se",           CFeature_table_reader_imp::eQual_snp_het_se           ),
    TQualKey ( "snp_linkout",          CFeature_table_reader_imp::eQual_snp_linkout          ),
    TQualKey ( "snp_maxrate",          CFeature_table_reader_imp::eQual_snp_maxrate          ),
    TQualKey ( "snp_valid",            CFeature_table_reader_imp::eQual_snp_valid            ),
    TQualKey ( "standard_name",        CFeature_table_reader_imp::eQual_standard_name        ),
    TQualKey ( "sts_aliases",          CFeature_table_reader_imp::eQual_sts_aliases          ),
    TQualKey ( "sts_dsegs",            CFeature_table_reader_imp::eQual_sts_dsegs            ),
    TQualKey ( "tag_peptide",          CFeature_table_reader_imp::eQual_tag_peptide          ),
    TQualKey ( "trans_splicing",       CFeature_table_reader_imp::eQual_trans_splicing       ),
    TQualKey ( "transcript_id",        CFeature_table_reader_imp::eQual_transcript_id        ),
    TQualKey ( "transcription",        CFeature_table_reader_imp::eQual_transcription        ),
    TQualKey ( "transl_except",        CFeature_table_reader_imp::eQual_transl_except        ),
    TQualKey ( "transl_table",         CFeature_table_reader_imp::eQual_transl_table         ),
    TQualKey ( "translation",          CFeature_table_reader_imp::eQual_translation          ),
    TQualKey ( "transposon",           CFeature_table_reader_imp::eQual_transposon           ),
    TQualKey ( "usedin",               CFeature_table_reader_imp::eQual_usedin               ),
    TQualKey ( "weight",               CFeature_table_reader_imp::eQual_weight               )
};

typedef CStaticArrayMap <const char*, const CFeature_table_reader_imp::EQual, PCase_CStr> TQualMap;
DEFINE_STATIC_ARRAY_MAP(TQualMap, sm_QualKeys, qual_key_to_subtype);


typedef pair <const char *, const CFeature_table_reader_imp::EOrgRef> TOrgRefKey;

static const TOrgRefKey orgref_key_to_subtype [] = {
    TOrgRefKey ( "div",        CFeature_table_reader_imp::eOrgRef_div       ),
    TOrgRefKey ( "gcode",      CFeature_table_reader_imp::eOrgRef_gcode     ),
    TOrgRefKey ( "lineage",    CFeature_table_reader_imp::eOrgRef_lineage   ),
    TOrgRefKey ( "mgcode",     CFeature_table_reader_imp::eOrgRef_mgcode    ),
    TOrgRefKey ( "organelle",  CFeature_table_reader_imp::eOrgRef_organelle ),
    TOrgRefKey ( "organism",   CFeature_table_reader_imp::eOrgRef_organism  )
};

typedef CStaticArrayMap <const char*, const CFeature_table_reader_imp::EOrgRef, PCase_CStr> TOrgRefMap;
DEFINE_STATIC_ARRAY_MAP(TOrgRefMap, sm_OrgRefKeys, orgref_key_to_subtype);


typedef pair <const char *, const CBioSource::EGenome> TGenomeKey;

static const TGenomeKey genome_key_to_subtype [] = {
    TGenomeKey ( "apicoplast",                CBioSource::eGenome_apicoplast       ),
    TGenomeKey ( "chloroplast",               CBioSource::eGenome_chloroplast      ),
    TGenomeKey ( "chromatophore",             CBioSource::eGenome_chromatophore    ),
    TGenomeKey ( "chromoplast",               CBioSource::eGenome_chromoplast      ),
    TGenomeKey ( "chromosome",                CBioSource::eGenome_chromosome       ),
    TGenomeKey ( "cyanelle",                  CBioSource::eGenome_cyanelle         ),
    TGenomeKey ( "endogenous_virus",          CBioSource::eGenome_endogenous_virus ),
    TGenomeKey ( "extrachrom",                CBioSource::eGenome_extrachrom       ),
    TGenomeKey ( "genomic",                   CBioSource::eGenome_genomic          ),
    TGenomeKey ( "hydrogenosome",             CBioSource::eGenome_hydrogenosome    ),
    TGenomeKey ( "insertion_seq",             CBioSource::eGenome_insertion_seq    ),
    TGenomeKey ( "kinetoplast",               CBioSource::eGenome_kinetoplast      ),
    TGenomeKey ( "leucoplast",                CBioSource::eGenome_leucoplast       ),
    TGenomeKey ( "macronuclear",              CBioSource::eGenome_macronuclear     ),
    TGenomeKey ( "mitochondrion",             CBioSource::eGenome_mitochondrion    ),
    TGenomeKey ( "mitochondrion:kinetoplast", CBioSource::eGenome_kinetoplast      ),
    TGenomeKey ( "nucleomorph",               CBioSource::eGenome_nucleomorph      ),
    TGenomeKey ( "plasmid",                   CBioSource::eGenome_plasmid          ),
    TGenomeKey ( "plastid",                   CBioSource::eGenome_plastid          ),
    TGenomeKey ( "plastid:apicoplast",        CBioSource::eGenome_apicoplast       ),
    TGenomeKey ( "plastid:chloroplast",       CBioSource::eGenome_chloroplast      ),
    TGenomeKey ( "plastid:chromoplast",       CBioSource::eGenome_chromoplast      ),
    TGenomeKey ( "plastid:cyanelle",          CBioSource::eGenome_cyanelle         ),
    TGenomeKey ( "plastid:leucoplast",        CBioSource::eGenome_leucoplast       ),
    TGenomeKey ( "plastid:proplastid",        CBioSource::eGenome_proplastid       ),
    TGenomeKey ( "proplastid",                CBioSource::eGenome_proplastid       ),
    TGenomeKey ( "proviral",                  CBioSource::eGenome_proviral         ),
    TGenomeKey ( "transposon",                CBioSource::eGenome_transposon       ),
    TGenomeKey ( "unknown",                   CBioSource::eGenome_unknown          ),
    TGenomeKey ( "virion",                    CBioSource::eGenome_virion           )
};

typedef CStaticArrayMap <const char*, const CBioSource::EGenome, PCase_CStr> TGenomeMap;
DEFINE_STATIC_ARRAY_MAP(TGenomeMap, sm_GenomeKeys, genome_key_to_subtype);


typedef pair <const char *, const CSubSource::ESubtype> TSubSrcKey;

static const TSubSrcKey subsrc_key_to_subtype [] = {
    TSubSrcKey ( "cell_line",            CSubSource::eSubtype_cell_line             ),
    TSubSrcKey ( "cell_type",            CSubSource::eSubtype_cell_type             ),
    TSubSrcKey ( "chromosome",           CSubSource::eSubtype_chromosome            ),
    TSubSrcKey ( "clone",                CSubSource::eSubtype_clone                 ),
    TSubSrcKey ( "clone_lib",            CSubSource::eSubtype_clone_lib             ),
    TSubSrcKey ( "collected_by",         CSubSource::eSubtype_collected_by          ),
    TSubSrcKey ( "collection_date",      CSubSource::eSubtype_collection_date       ),
    TSubSrcKey ( "country",              CSubSource::eSubtype_country               ),
    TSubSrcKey ( "dev_stage",            CSubSource::eSubtype_dev_stage             ),
    TSubSrcKey ( "endogenous_virus",     CSubSource::eSubtype_endogenous_virus_name ),
    TSubSrcKey ( "environmental_sample", CSubSource::eSubtype_environmental_sample  ),
    TSubSrcKey ( "frequency",            CSubSource::eSubtype_frequency             ),
    TSubSrcKey ( "fwd_primer_name",      CSubSource::eSubtype_fwd_primer_name       ),
    TSubSrcKey ( "fwd_primer_seq",       CSubSource::eSubtype_fwd_primer_seq        ),
    TSubSrcKey ( "genotype",             CSubSource::eSubtype_genotype              ),
    TSubSrcKey ( "germline",             CSubSource::eSubtype_germline              ),
    TSubSrcKey ( "haplotype",            CSubSource::eSubtype_haplotype             ),
    TSubSrcKey ( "identified_by",        CSubSource::eSubtype_identified_by         ),
    TSubSrcKey ( "insertion_seq",        CSubSource::eSubtype_insertion_seq_name    ),
    TSubSrcKey ( "isolation_source",     CSubSource::eSubtype_isolation_source      ),
    TSubSrcKey ( "lab_host",             CSubSource::eSubtype_lab_host              ),
    TSubSrcKey ( "lat_lon",              CSubSource::eSubtype_lat_lon               ),
    TSubSrcKey ( "map",                  CSubSource::eSubtype_map                   ),
    TSubSrcKey ( "metagenomic",          CSubSource::eSubtype_metagenomic           ),
    TSubSrcKey ( "plasmid",              CSubSource::eSubtype_plasmid_name          ),
    TSubSrcKey ( "plastid",              CSubSource::eSubtype_plastid_name          ),
    TSubSrcKey ( "pop_variant",          CSubSource::eSubtype_pop_variant           ),
    TSubSrcKey ( "rearranged",           CSubSource::eSubtype_rearranged            ),
    TSubSrcKey ( "rev_primer_name",      CSubSource::eSubtype_rev_primer_name       ),
    TSubSrcKey ( "rev_primer_seq",       CSubSource::eSubtype_rev_primer_seq        ),
    TSubSrcKey ( "segment",              CSubSource::eSubtype_segment               ),
    TSubSrcKey ( "sex",                  CSubSource::eSubtype_sex                   ),
    TSubSrcKey ( "subclone",             CSubSource::eSubtype_subclone              ),
    TSubSrcKey ( "tissue_lib ",          CSubSource::eSubtype_tissue_lib            ),
    TSubSrcKey ( "tissue_type",          CSubSource::eSubtype_tissue_type           ),
    TSubSrcKey ( "transgenic",           CSubSource::eSubtype_transgenic            ),
    TSubSrcKey ( "transposon",           CSubSource::eSubtype_transposon_name       )
};

typedef CStaticArrayMap <const char*, const CSubSource::ESubtype, PCase_CStr> TSubSrcMap;
DEFINE_STATIC_ARRAY_MAP(TSubSrcMap, sm_SubSrcKeys, subsrc_key_to_subtype);


typedef pair <const char *, const COrgMod::ESubtype> TOrgModKey;

static const TOrgModKey orgmod_key_to_subtype [] = {
    TOrgModKey ( "acronym",            COrgMod::eSubtype_acronym            ),
    TOrgModKey ( "anamorph",           COrgMod::eSubtype_anamorph           ),
    TOrgModKey ( "authority",          COrgMod::eSubtype_authority          ),
    TOrgModKey ( "bio_material",       COrgMod::eSubtype_bio_material       ),
    TOrgModKey ( "biotype",            COrgMod::eSubtype_biotype            ),
    TOrgModKey ( "biovar",             COrgMod::eSubtype_biovar             ),
    TOrgModKey ( "breed",              COrgMod::eSubtype_breed              ),
    TOrgModKey ( "chemovar",           COrgMod::eSubtype_chemovar           ),
    TOrgModKey ( "common",             COrgMod::eSubtype_common             ),
    TOrgModKey ( "cultivar",           COrgMod::eSubtype_cultivar           ),
    TOrgModKey ( "culture_collection", COrgMod::eSubtype_culture_collection ),
    TOrgModKey ( "dosage",             COrgMod::eSubtype_dosage             ),
    TOrgModKey ( "ecotype",            COrgMod::eSubtype_ecotype            ),
    TOrgModKey ( "forma",              COrgMod::eSubtype_forma              ),
    TOrgModKey ( "forma_specialis",    COrgMod::eSubtype_forma_specialis    ),
    TOrgModKey ( "gb_acronym",         COrgMod::eSubtype_gb_acronym         ),
    TOrgModKey ( "gb_anamorph",        COrgMod::eSubtype_gb_anamorph        ),
    TOrgModKey ( "gb_synonym",         COrgMod::eSubtype_gb_synonym         ),
    TOrgModKey ( "group",              COrgMod::eSubtype_group              ),
    TOrgModKey ( "isolate",            COrgMod::eSubtype_isolate            ),
    TOrgModKey ( "metagenome_source",  COrgMod::eSubtype_metagenome_source  ),
    TOrgModKey ( "nat_host",           COrgMod::eSubtype_nat_host           ),
    TOrgModKey ( "natural_host",       COrgMod::eSubtype_nat_host           ),
    TOrgModKey ( "old_lineage",        COrgMod::eSubtype_old_lineage        ),
    TOrgModKey ( "old_name",           COrgMod::eSubtype_old_name           ),
    TOrgModKey ( "pathovar",           COrgMod::eSubtype_pathovar           ),
    TOrgModKey ( "serogroup",          COrgMod::eSubtype_serogroup          ),
    TOrgModKey ( "serotype",           COrgMod::eSubtype_serotype           ),
    TOrgModKey ( "serovar",            COrgMod::eSubtype_serovar            ),
    TOrgModKey ( "spec_host",          COrgMod::eSubtype_nat_host           ),
    TOrgModKey ( "specific_host",      COrgMod::eSubtype_nat_host           ),
    TOrgModKey ( "specimen_voucher",   COrgMod::eSubtype_specimen_voucher   ),
    TOrgModKey ( "strain",             COrgMod::eSubtype_strain             ),
    TOrgModKey ( "sub_species",        COrgMod::eSubtype_sub_species        ),
    TOrgModKey ( "subgroup",           COrgMod::eSubtype_subgroup           ),
    TOrgModKey ( "substrain",          COrgMod::eSubtype_substrain          ),
    TOrgModKey ( "subtype",            COrgMod::eSubtype_subtype            ),
    TOrgModKey ( "synonym",            COrgMod::eSubtype_synonym            ),
    TOrgModKey ( "teleomorph",         COrgMod::eSubtype_teleomorph         ),
    TOrgModKey ( "type",               COrgMod::eSubtype_type               ),
    TOrgModKey ( "variety",            COrgMod::eSubtype_variety            )
};

typedef CStaticArrayMap <const char*, const COrgMod::ESubtype, PCase_CStr> TOrgModMap;
DEFINE_STATIC_ARRAY_MAP(TOrgModMap, sm_OrgModKeys, orgmod_key_to_subtype);


typedef pair <const char *, const int> TTrnaKey;

static const TTrnaKey trna_key_to_subtype [] = {
    TTrnaKey ( "Ala",            'A' ),
    TTrnaKey ( "Alanine",        'A' ),
    TTrnaKey ( "Arg",            'R' ),
    TTrnaKey ( "Arginine",       'R' ),
    TTrnaKey ( "Asn",            'N' ),
    TTrnaKey ( "Asp",            'D' ),
    TTrnaKey ( "Asp or Asn",     'B' ),
    TTrnaKey ( "Asparagine",     'N' ),
    TTrnaKey ( "Aspartate",      'D' ),
    TTrnaKey ( "Aspartic Acid",  'D' ),
    TTrnaKey ( "Asx",            'B' ),
    TTrnaKey ( "Cys",            'C' ),
    TTrnaKey ( "Cysteine",       'C' ),
    TTrnaKey ( "Gln",            'Q' ),
    TTrnaKey ( "Glu",            'E' ),
    TTrnaKey ( "Glu or Gln",     'Z' ),
    TTrnaKey ( "Glutamate",      'E' ),
    TTrnaKey ( "Glutamic Acid",  'E' ),
    TTrnaKey ( "Glutamine",      'Q' ),
    TTrnaKey ( "Glx",            'Z' ),
    TTrnaKey ( "Gly",            'G' ),
    TTrnaKey ( "Glycine",        'G' ),
    TTrnaKey ( "His",            'H' ),
    TTrnaKey ( "Histidine",      'H' ),
    TTrnaKey ( "Ile",            'I' ),
    TTrnaKey ( "Isoleucine",     'I' ),
    TTrnaKey ( "Leu",            'L' ),
    TTrnaKey ( "Leu or Ile",     'J' ),
    TTrnaKey ( "Leucine",        'L' ),
    TTrnaKey ( "Lys",            'K' ),
    TTrnaKey ( "Lysine",         'K' ),
    TTrnaKey ( "Met",            'M' ),
    TTrnaKey ( "Methionine",     'M' ),
    TTrnaKey ( "OTHER",          'X' ),
    TTrnaKey ( "Phe",            'F' ),
    TTrnaKey ( "Phenylalanine",  'F' ),
    TTrnaKey ( "Pro",            'P' ),
    TTrnaKey ( "Proline",        'P' ),
    TTrnaKey ( "Pyl",            'O' ),
    TTrnaKey ( "Pyrrolysine",    'O' ),
    TTrnaKey ( "Sec",            'U' ),
    TTrnaKey ( "Selenocysteine", 'U' ),
    TTrnaKey ( "Ser",            'S' ),
    TTrnaKey ( "Serine",         'S' ),
    TTrnaKey ( "TERM",           '*' ),
    TTrnaKey ( "Ter",            '*' ),
    TTrnaKey ( "Termination",    '*' ),
    TTrnaKey ( "Thr",            'T' ),
    TTrnaKey ( "Threonine",      'T' ),
    TTrnaKey ( "Trp",            'W' ),
    TTrnaKey ( "Tryptophan",     'W' ),
    TTrnaKey ( "Tyr",            'Y' ),
    TTrnaKey ( "Tyrosine",       'Y' ),
    TTrnaKey ( "Val",            'V' ),
    TTrnaKey ( "Valine",         'V' ),
    TTrnaKey ( "Xle",            'J' ),
    TTrnaKey ( "Xxx",            'X' ),
    TTrnaKey ( "fMet",           'M' )
};

typedef CStaticArrayMap <const char*, const int, PCase_CStr> TTrnaMap;
DEFINE_STATIC_ARRAY_MAP(TTrnaMap, sm_TrnaKeys, trna_key_to_subtype);


static const char * const single_key_list [] = {
    "environmental_sample",
    "germline",
    "metagenomic",
    "partial",
    "pseudo",
    "rearranged",
    "ribosomal_slippage",
    "trans_splicing",
    "transgenic"
};

typedef CStaticArraySet <const char*, PCase_CStr> TSingleSet;
DEFINE_STATIC_ARRAY_MAP(TSingleSet, sc_SingleKeys, single_key_list);


// constructor
CFeature_table_reader_imp::CFeature_table_reader_imp(void)
{
}

// destructor
CFeature_table_reader_imp::~CFeature_table_reader_imp(void)
{
}


bool CFeature_table_reader_imp::x_ParseFeatureTableLine (
    const string& line,
    Int4* startP,
    Int4* stopP,
    bool* partial5P,
    bool* partial3P,
    bool* ispointP,
    bool* isminusP,
    string& featP,
    string& qualP,
    string& valP,
    Int4 offset,

    IErrorContainer *container, 
    int line_num, 
    const string &seq_id
)

{
    SIZE_TYPE      numtkns;
    bool           badNumber = false;
    bool           isminus = false;
    bool           ispoint = false;
    size_t         len;
    bool           partial5 = false;
    bool           partial3 = false;
    Int4           startv = -1;
    Int4           stopv = -1;
    Int4           swp;
    string         start, stop, feat, qual, val, stnd;
    vector<string> tkns;

    if (line.empty ()) return false;

    /* offset and other instructions encoded in brackets */
    if (NStr::StartsWith (line, '[')) return false;

    tkns.clear ();
    x_TokenizeLenient(line, tkns);
    numtkns = tkns.size ();

    if (numtkns > 0) {
        start = NStr::TruncateSpaces(tkns[0]);
    }
    if (numtkns > 1) {
        stop = NStr::TruncateSpaces(tkns[1]);
    }
    if (numtkns > 2) {
        feat = NStr::TruncateSpaces(tkns[2]);
    }
    if (numtkns > 3) {
        qual = NStr::TruncateSpaces(tkns[3]);
    }
    if (numtkns > 4) {
        val = NStr::TruncateSpaces(tkns[4]);
    }
    if (numtkns > 5) {
        stnd = NStr::TruncateSpaces(tkns[5]);
    }

    if (! start.empty ()) {
        if (start [0] == '<') {
            partial5 = true;
            start.erase (0, 1);
        }
        len = start.length ();
        if (len > 1 && start [len - 1] == '^') {
          ispoint = true;
          start [len - 1] = '\0';
        }
        try {
            startv = x_StringToLongNoThrow(start, container, seq_id, line_num, feat, qual,
                ILineError::eProblem_BadFeatureInterval);
        } catch (...) {
            badNumber = true;
        }
    }

    if (! stop.empty ()) {
        if (stop [0] == '>') {
            partial3 = true;
            stop.erase (0, 1);
        }
        try {
            stopv = x_StringToLongNoThrow (stop, container, seq_id, line_num, feat, qual,
                ILineError::eProblem_BadFeatureInterval);
        } catch (CStringException) {
            badNumber = true;
        }
    }

    if (badNumber) {
        startv = -1;
        stopv = -1;
    } else {
        startv--;
        stopv--;
        if (! stnd.empty ()) {
            if (stnd == "minus" || stnd == "-" || stnd == "complement") {
                if (start < stop) {
                    swp = startv;
                    startv = stopv;
                    stopv = swp;
                }
                isminus = true;
            }
        }
    }

    *startP = startv + offset;
    *stopP = stopv + offset;
    *partial5P = partial5;
    *partial3P = partial3;
    *ispointP = ispoint;
    *isminusP = isminus;
    featP = feat;
    qualP = qual;
    valP = val;

    return true;
}

void CFeature_table_reader_imp::x_TokenizeStrict( 
    const string &line, 
    vector<string> &out_tokens )
{
    out_tokens.clear();

    // each token has spaces before it and a tab or end-of-line after it
    string::size_type startPosOfNextRoundOfTokenization = 0;
    while ( startPosOfNextRoundOfTokenization < line.size() ) {
        const string::size_type posAfterSpaces = line.find_first_not_of( ' ', startPosOfNextRoundOfTokenization );
        if( posAfterSpaces == string::npos ) {
            return;
        }

        string::size_type posOfTab = line.find( '\t', posAfterSpaces );
        if( posOfTab == string::npos ) {
            posOfTab = line.length();
        }

        // The next token is between the spaces and the tab (or end of string)
        out_tokens.push_back(kEmptyStr);
        string &new_token = out_tokens.back();
        copy( line.begin() + posAfterSpaces, line.begin() + posOfTab, back_inserter(new_token) );
        NStr::TruncateSpacesInPlace( new_token );

        startPosOfNextRoundOfTokenization = ( posOfTab + 1 );
    }
}

// since some compilers won't let me use isspace for find_if
class CIsSpace {
public:
    bool operator()( char c ) { return isspace(c); }
};

class CIsNotSpace {
public:
    bool operator()( char c ) { return ! isspace(c); }
};

void CFeature_table_reader_imp::x_TokenizeLenient( 
    const string &line, 
    vector<string> &out_tokens )
{
    out_tokens.clear();

    if( line.empty() ) {
        return;
    }

    // if it starts with whitespace, it must be a qual line, else it's a feature line
    if( isspace(line[0]) ) {
        // In regex form, we're doing something like this:
        // \s+(\S+)(\s+(\S.*))?
        // Where the first is the qual, and the rest is the val
        const string::const_iterator start_of_qual = find_if( line.begin(), line.end(), CIsNotSpace() );
        if( start_of_qual == line.end() ) {
            return;
        }
        const string::const_iterator start_of_whitespace_after_qual = find_if( start_of_qual, line.end(), CIsSpace() );
        const string::const_iterator start_of_val = find_if( start_of_whitespace_after_qual, line.end(), CIsNotSpace() );

        // first 3 are empty
        out_tokens.push_back(kEmptyStr);
        out_tokens.push_back(kEmptyStr);
        out_tokens.push_back(kEmptyStr);

        // then qual
        out_tokens.push_back(kEmptyStr);
        string &qual = out_tokens.back();
        copy( start_of_qual, start_of_whitespace_after_qual, back_inserter(qual) );

        // then val
        if( start_of_val != line.end() ) {
            out_tokens.push_back(kEmptyStr);
            string &val = out_tokens.back();
            copy( start_of_val, line.end(), back_inserter(val) );
            NStr::TruncateSpacesInPlace( val );
        }

    } else {
        // parse a feature line

        // Since we're being lenient, we consider it to be 3 ( or 6 ) parts separated by whitespace
        const string::const_iterator first_column_start = line.begin();
        const string::const_iterator first_whitespace = find_if( first_column_start, line.end(), CIsSpace() );
        const string::const_iterator second_column_start = find_if( first_whitespace, line.end(), CIsNotSpace() );
        const string::const_iterator second_whitespace = find_if( second_column_start, line.end(), CIsSpace() );
        const string::const_iterator third_column_start = find_if( second_whitespace, line.end(), CIsNotSpace() );
        const string::const_iterator third_whitespace = find_if( third_column_start, line.end(), CIsSpace() );
        // columns 4 and 5 are unused on feature lines
        const string::const_iterator sixth_column_start = find_if( third_whitespace, line.end(), CIsNotSpace() );
        const string::const_iterator sixth_whitespace = find_if( sixth_column_start, line.end(), CIsSpace() );

        out_tokens.push_back(kEmptyStr);
        string &first = out_tokens.back();
        copy( first_column_start, first_whitespace, back_inserter(first) );

        out_tokens.push_back(kEmptyStr);
        string &second = out_tokens.back();
        copy( second_column_start, second_whitespace, back_inserter(second) );

        out_tokens.push_back(kEmptyStr);
        string &third = out_tokens.back();
        copy( third_column_start, third_whitespace, back_inserter(third) );

        if( sixth_column_start != line.end() ) {
            // columns 4 and 5 are unused
            out_tokens.push_back(kEmptyStr);
            out_tokens.push_back(kEmptyStr);

            out_tokens.push_back(kEmptyStr);
            string &sixth = out_tokens.back();
            copy( sixth_column_start, sixth_whitespace, back_inserter(sixth) );
        }
    }
}


bool CFeature_table_reader_imp::x_AddQualifierToGene (
    CSeqFeatData& sfdata,
    EQual qtype,
    const string& val
)

{
    CGene_ref& grp = sfdata.SetGene ();
    switch (qtype) {
        case eQual_gene:
            grp.SetLocus (val);
            return true;
        case eQual_allele:
            grp.SetAllele (val);
            return true;
        case eQual_gene_desc:
            grp.SetDesc (val);
            return true;
        case eQual_gene_syn:
            {
                CGene_ref::TSyn& syn = grp.SetSyn ();
                syn.push_back (val);
                return true;
            }
        case eQual_map:
            grp.SetMaploc (val);
            return true;
        case eQual_locus_tag:
            grp.SetLocus_tag (val);
            return true;
        case eQual_nomenclature:
            /* !!! need to implement !!! */
            return true;
        default:
            break;
    }
    return false;
}


bool CFeature_table_reader_imp::x_AddQualifierToCdregion (
    CRef<CSeq_feat> sfp,
    CSeqFeatData& sfdata,
    EQual qtype, const string& val,
    IErrorContainer *container, 
    int line, 
    const string &seq_id 
)

{
    CCdregion& crp = sfdata.SetCdregion ();
    switch (qtype) {
        case eQual_codon_start:
            {
                int frame = x_StringToLongNoThrow (val, container, seq_id, line, "CDS", "codon_start");
                switch (frame) {
                    case 0:
                        crp.SetFrame (CCdregion::eFrame_not_set);
                        break;
                    case 1:
                        crp.SetFrame (CCdregion::eFrame_one);
                        break;
                    case 2:
                        crp.SetFrame (CCdregion::eFrame_two);
                        break;
                    case 3:
                        crp.SetFrame (CCdregion::eFrame_three);
                        break;
                    default:
                        break;
                }
                return true;
            }
        case eQual_EC_number:
            {
                CProt_ref& prp = sfp->SetProtXref ();
                CProt_ref::TEc& ec = prp.SetEc ();
                ec.push_back (val);
                return true;
            }
        case eQual_function:
            {
                CProt_ref& prp = sfp->SetProtXref ();
                CProt_ref::TActivity& fun = prp.SetActivity ();
                fun.push_back (val);
                return true;
            }
        case eQual_product:
            {
                CProt_ref& prp = sfp->SetProtXref ();
                CProt_ref::TName& prod = prp.SetName ();
                prod.push_back (val);
                return true;
            }
        case eQual_prot_desc:
            {
                CProt_ref& prp = sfp->SetProtXref ();
                prp.SetDesc (val);
                return true;
            }
        case eQual_prot_note:
            return x_AddGBQualToFeature(sfp, "prot_note", val);
        case eQual_transl_except:
            /* !!! */
            /* if (x_ParseCodeBreak (sfp, crp, val)) return true; */
            return true;
        case eQual_translation:
            // we should accept, but ignore this qual on CDSs.
            // so, do nothing but return success
            return true;
        default:
            break;
    }
    return false;
}


bool CFeature_table_reader_imp::x_StringIsJustQuotes (
    const string& str
)

{
    ITERATE (string, it, str) {
      char ch = *it;
      if (ch > ' ' && ch != '"' && ch != '\'') return false;
    }

    return true;
}

// returns true if this is a feature line.
// If it's a feature line it also repairs it if necessary
static bool 
s_IsFeatureLineAndFix (
    CTempString& line)
{
    // this is a Feature line if the first 
    // non-space character is a '>'

    ITERATE(CTempString, str_iter, line) {
        const char ch = *str_iter;
        if( ! isspace(ch) ) {
            if(ch == '>') {
                if( str_iter != line.begin() ) {
                    line = NStr::TruncateSpaces(line, NStr::eTrunc_Begin);
                }
                return true;
            } else {
                return false;
            }
        }
    }
    return false;
}

static bool
s_LineIndicatesOrder( const string & line )
{
    // basically, this is true if the line starts with "order" (whitespaces disregarded)

    const static string kOrder("ORDER");

    // find first non-whitespace character
    string::size_type pos = 0;
    for( ; pos < line.length() && isspace(line[pos]); ++pos) {
        // nothing to do here
    }

    // line is all whitespace
    if( pos >= line.length() ) {
        return false;
    }

    // check if starts with "order" after whitespace
    return ( 0 == NStr::CompareNocase( line, pos, kOrder.length(), kOrder ) );
}

// Turns a "join" location into an "order" by putting nulls between it
// Returns an unset CRef if the loc doesn't need nulls (e.g. if it's just an interval)
static CRef<CSeq_loc>
s_LocationJoinToOrder( const CSeq_loc & loc )
{
    // create result we're returning
    CRef<CSeq_loc> result( new CSeq_loc );
    CSeq_loc_mix::Tdata & mix_pieces  = result->SetMix().Set();

    // keep this around for whenever we need a "null" piece
    CRef<CSeq_loc> loc_piece_null( new CSeq_loc );
    loc_piece_null->SetNull();

    // push pieces of source, with NULLs between
    CSeq_loc_CI loc_iter( loc );
    for( ; loc_iter; ++loc_iter ) {
        if( ! mix_pieces.empty() ) {
            mix_pieces.push_back( loc_piece_null );
        }
        CRef<CSeq_loc> new_piece( new CSeq_loc );
        new_piece->Assign( loc_iter.GetEmbeddingSeq_loc() );
        mix_pieces.push_back( new_piece );
    }

    // Only wrap in "mix" if there was more than one piece
    if( mix_pieces.size() > 1 ) {
        return result;
    } else {
        return CRef<CSeq_loc>();
    }
}

int CFeature_table_reader_imp::x_ParseTrnaString (
    const string& val
)

{
    string fst, scd;

    scd = val;
    if (NStr::StartsWith (val, "tRNA-")) {
        NStr::SplitInTwo (val, "-", fst, scd);
    }

    TTrnaMap::const_iterator t_iter = sm_TrnaKeys.find (scd.c_str ());
    if (t_iter != sm_TrnaKeys.end ()) {
        return t_iter->second;
    }

    return 0;
}

void
CFeature_table_reader_imp::x_ParseTrnaExtString(
    CTrna_ext & ext_trna, const string & str, const CSeq_id *seq_id )
{
    if (NStr::IsBlank (str)) return;

    if (NStr::StartsWith (str, "(pos:")) {
        // find position of closing paren
        string::size_type pos_end = x_MatchingParenPos( str, 0 );
        if (pos_end != string::npos) {
            string pos_str = str.substr (5, pos_end - 5);
            string::size_type aa_start = NStr::FindNoCase (pos_str, "aa:");
            if (aa_start != string::npos) {
                string abbrev = pos_str.substr (aa_start + 3);
                TTrnaMap::const_iterator t_iter = sm_TrnaKeys.find (abbrev.c_str ());
                if (t_iter == sm_TrnaKeys.end ()) {
                    // unable to parse
                    return;
                }
                CRef<CTrna_ext::TAa> aa(new CTrna_ext::TAa);
                aa->SetNcbieaa (t_iter->second);
                ext_trna.SetAa(*aa);
                pos_str = pos_str.substr (0, aa_start);
                NStr::TruncateSpacesInPlace (pos_str);
                if (NStr::EndsWith (pos_str, ",")) {
                    pos_str = pos_str.substr (0, pos_str.length() - 1);
                }
            }
            CGetSeqLocFromStringHelper helper;
            CRef<CSeq_loc> anticodon = GetSeqLocFromString (pos_str, seq_id, & helper);
            if (anticodon == NULL) {
                ext_trna.ResetAa();
            } else {
                ext_trna.SetAnticodon(*anticodon);
            }
        }
    }
}


SIZE_TYPE CFeature_table_reader_imp::x_MatchingParenPos( 
    const string &str, SIZE_TYPE open_paren_pos )
{
    _ASSERT( str[open_paren_pos] == '(' );
    _ASSERT( open_paren_pos < str.length() );

    // nesting level. start at 1 since we know there's an open paren
    int level = 1;

    SIZE_TYPE pos = open_paren_pos + 1;
    for( ; pos < str.length(); ++pos ) {
        switch( str[pos] ) {
            case '(':
                // nesting deeper
                ++level;
                break;
            case ')':
                // closed a level of nesting
                --level;
                if( 0 == level ) {
                    // reached the top: we're closing the initial paren,
                    // so we return our position
                    return pos;
                }
                break;
            default:
                // ignore other characters.
                // maybe in the future we'll handle ignoring parens in quotes or
                // things like that.
                break;
        }
    }
    return NPOS;
}

long CFeature_table_reader_imp::x_StringToLongNoThrow (
    CTempString strToConvert,
    IErrorContainer *container, 
    const std::string& strSeqId,
    unsigned int uLine,
    CTempString strFeatureName,
    CTempString strQualifierName,
    ILineError::EProblem eProblem
)
{
    try {
        return NStr::StringToLong(strToConvert);
    } catch( ... ) {
        // See if we start with a number, but there's extra junk after it, try again
        if( ! strToConvert.empty() && isdigit(strToConvert[0]) ) {
            try {
                long result = NStr::StringToLong(strToConvert, NStr::fAllowTrailingSymbols);

                ILineError::EProblem problem = 
                    ILineError::eProblem_NumericQualifierValueHasExtraTrailingCharacters;
                if( eProblem != ILineError::eProblem_Unset ) {
                    problem = eProblem;
                }

                x_ProcessMsg( container, 
                    problem,
                    eDiag_Warning,
                    strSeqId, uLine, strFeatureName, strQualifierName, strToConvert );
                return result;
            } catch( ... ) { } // fall-thru to usual handling
        }

        ILineError::EProblem problem = 
            ILineError::eProblem_NumericQualifierValueIsNotANumber;
        if( eProblem != ILineError::eProblem_Unset ) {
            problem = eProblem;
        }

        x_ProcessMsg( container,
            problem,
            eDiag_Warning,
            strSeqId, uLine, strFeatureName, strQualifierName, strToConvert );
        // we have no idea, so just return zero
        return 0;
    }
}


bool CFeature_table_reader_imp::x_AddQualifierToRna (
    CSeqFeatData& sfdata,
    EQual qtype,
    const string& val,
    IErrorContainer *container, 
    int line_num, 
    const string &seq_id
)
{
    CRNA_ref& rrp = sfdata.SetRna ();
    CRNA_ref::EType rnatyp = rrp.GetType ();
    switch (rnatyp) {
        case CRNA_ref::eType_premsg:
        case CRNA_ref::eType_mRNA:
        case CRNA_ref::eType_rRNA:
            switch (qtype) {
                case eQual_product:
                    {
                        CRNA_ref::TExt& tex = rrp.SetExt ();
                        CRNA_ref::C_Ext::E_Choice exttype = tex.Which ();
                        if (exttype == CRNA_ref::C_Ext::e_TRNA) return false;
                        tex.SetName (val);
                        return true;
                    }
                default:
                    break;
            }
            break;
        case CRNA_ref::eType_snRNA:
        case CRNA_ref::eType_scRNA:
        case CRNA_ref::eType_snoRNA:
        case CRNA_ref::eType_other:
            return false;
        case CRNA_ref::eType_tRNA:
            switch (qtype) {
                case eQual_product: {
                        CRNA_ref::TExt& tex = rrp.SetExt ();
                        CRNA_ref::C_Ext::E_Choice exttype = tex.Which ();
                        if (exttype == CRNA_ref::C_Ext::e_Name) return false;
                        CTrna_ext& trx = tex.SetTRNA ();
                        int aaval = x_ParseTrnaString (val);
                        if (aaval > 0) {
                            CTrna_ext::TAa& taa = trx.SetAa ();
                            taa.SetNcbieaa (aaval);
                            trx.SetAa (taa);
                            tex.SetTRNA (trx);
                            return true;
                        }
                    }
                    break;
                case eQual_anticodon:
                    {
                        CRNA_ref::TExt& tex = rrp.SetExt ();
                        CRNA_ref::C_Ext::TTRNA & ext_trna = tex.SetTRNA();
                        CRef<CSeq_id> seq_id_obj( new CSeq_id(seq_id) );
                        x_ParseTrnaExtString(ext_trna, val, &*seq_id_obj);
                    }
                    break;
                case eQual_codon_recognized: 
                    {
                        CRNA_ref::TExt& tex = rrp.SetExt ();
                        CRNA_ref::C_Ext::TTRNA & ext_trna = tex.SetTRNA();
                        ext_trna.SetAa().SetNcbieaa();
                        ext_trna.SetCodon().push_back( CGen_code_table::CodonToIndex(val) );
                        return true;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return false;
}


bool CFeature_table_reader_imp::x_AddQualifierToImp (
    CRef<CSeq_feat> sfp,
    CSeqFeatData& sfdata,
    EQual qtype,
    const string& qual,
    const string& val
)

{
    const char *str = NULL;

    CSeqFeatData::ESubtype subtype = sfdata.GetSubtype ();
    switch (subtype) {
        case CSeqFeatData::eSubtype_variation:
            {
                switch (qtype) {
                    case eQual_chrcnt:
                    case eQual_ctgcnt:
                    case eQual_loccnt:
                    case eQual_snp_class:
                    case eQual_snp_gtype:
                    case eQual_snp_het:
                    case eQual_snp_het_se:
                    case eQual_snp_linkout:
                    case eQual_snp_maxrate:
                    case eQual_snp_valid:
                    case eQual_weight:
                        str = "dbSnpSynonymyData";
                        break;
                    default:
                        break;
                }
            }
            break;
        case CSeqFeatData::eSubtype_STS:
            {
                switch (qtype) {
                    case eQual_sts_aliases:
                    case eQual_sts_dsegs:
                    case eQual_weight:
                        str = "stsUserObject";
                        break;
                    default:
                        break;
                }
            }
            break;
        case CSeqFeatData::eSubtype_misc_feature:
            {
                switch (qtype) {
                    case eQual_bac_ends:
                    case eQual_clone_id:
                    case eQual_method:
                    case eQual_sequence:
                    case eQual_STS:
                    case eQual_weight:
                        str = "cloneUserObject";
                        break;
                    default:
                        break;
                }
            }
            break;
        default:
            break;
    }

    if( NULL != str ) {
        CSeq_feat::TExt& ext = sfp->SetExt ();
        CObject_id& obj = ext.SetType ();
        if ((! obj.IsStr ()) || obj.GetStr ().empty ()) {
            obj.SetStr ();
        }
        ext.AddField (qual, val, CUser_object::eParse_Number);
        return true;
    }

    return false;
}


bool CFeature_table_reader_imp::x_AddQualifierToBioSrc (
    CSeqFeatData& sfdata,
    const string &feat_name,
    EOrgRef rtype,
    const string& val,
    IErrorContainer *container, 
    int line, 	
    const string &seq_id 
)

{
    CBioSource& bsp = sfdata.SetBiosrc ();

    switch (rtype) {
        case eOrgRef_organism:
            {
                CBioSource::TOrg& orp = bsp.SetOrg ();
                orp.SetTaxname (val);
                return true;
            }
        case eOrgRef_organelle:
            {
                TGenomeMap::const_iterator g_iter = sm_GenomeKeys.find (val.c_str ());
                if (g_iter != sm_GenomeKeys.end ()) {
                    CBioSource::EGenome gtype = g_iter->second;
                    bsp.SetGenome (gtype);
                    return true;
                }
            }
        case eOrgRef_div:
            {
                CBioSource::TOrg& orp = bsp.SetOrg ();
                COrg_ref::TOrgname& onp = orp.SetOrgname ();
                onp.SetDiv (val);
                return true;
            }
        case eOrgRef_lineage:
            {
                CBioSource::TOrg& orp = bsp.SetOrg ();
                COrg_ref::TOrgname& onp = orp.SetOrgname ();
                onp.SetLineage (val);
                return true;
            }
        case eOrgRef_gcode:
            {
                CBioSource::TOrg& orp = bsp.SetOrg ();
                COrg_ref::TOrgname& onp = orp.SetOrgname ();
                int code = x_StringToLongNoThrow (val, container, seq_id, line, feat_name, "gcode");
                onp.SetGcode (code);
                return true;
            }
        case eOrgRef_mgcode:
            {
                CBioSource::TOrg& orp = bsp.SetOrg ();
                COrg_ref::TOrgname& onp = orp.SetOrgname ();
                int code = x_StringToLongNoThrow (val, container, seq_id, line, feat_name, "mgcode");
                onp.SetMgcode (code);
                return true;
            }
        default:
            break;
    }
    return false;
}


bool CFeature_table_reader_imp::x_AddQualifierToBioSrc (
    CSeqFeatData& sfdata,
    CSubSource::ESubtype stype,
    const string& val
)

{
    CBioSource& bsp = sfdata.SetBiosrc ();
    CBioSource::TSubtype& slist = bsp.SetSubtype ();
    CRef<CSubSource> ssp (new CSubSource);
    ssp->SetSubtype (stype);
    ssp->SetName (val);
    slist.push_back (ssp);
    return true;
}


bool CFeature_table_reader_imp::x_AddQualifierToBioSrc (
    CSeqFeatData& sfdata,
    COrgMod::ESubtype mtype,
    const string& val
)

{
    CBioSource& bsp = sfdata.SetBiosrc ();
    CBioSource::TOrg& orp = bsp.SetOrg ();
    COrg_ref::TOrgname& onp = orp.SetOrgname ();
    COrgName::TMod& mlist = onp.SetMod ();
    CRef<COrgMod> omp (new COrgMod);
    omp->SetSubtype (mtype);
    omp->SetSubname (val);
    mlist.push_back (omp);
    return true;
}


bool CFeature_table_reader_imp::x_AddGBQualToFeature (
    CRef<CSeq_feat> sfp,
    const string& qual,
    const string& val
)

{
    if (qual.empty ()) return false;

    CSeq_feat::TQual& qlist = sfp->SetQual ();
    CRef<CGb_qual> gbq (new CGb_qual);
    gbq->SetQual (qual);
    if (x_StringIsJustQuotes (val)) {
        gbq->SetVal (kEmptyStr);
    } else {
        gbq->SetVal (val);
    }
    qlist.push_back (gbq);

    return true;
}


bool CFeature_table_reader_imp::x_AddQualifierToFeature (
    CRef<CSeq_feat> sfp,
    const string &feat_name,
    const string& qual,
    const string& val,
    IErrorContainer *container, 
    int line, 	
    const string &seq_id 
)

{
    CSeqFeatData&          sfdata = sfp->SetData ();
    CSeqFeatData::E_Choice typ = sfdata.Which ();

    if (typ == CSeqFeatData::e_Biosrc) {

        TOrgRefMap::const_iterator o_iter = sm_OrgRefKeys.find (qual.c_str ());
        if (o_iter != sm_OrgRefKeys.end ()) {
            EOrgRef rtype = o_iter->second;
            if (x_AddQualifierToBioSrc (sfdata, feat_name, rtype, val, container, line, seq_id)) return true;

        } else {

            TSubSrcMap::const_iterator s_iter = sm_SubSrcKeys.find (qual.c_str ());
            if (s_iter != sm_SubSrcKeys.end ()) {

                CSubSource::ESubtype stype = s_iter->second;
                if (x_AddQualifierToBioSrc (sfdata, stype, val)) return true;

            } else {

                TOrgModMap::const_iterator m_iter = sm_OrgModKeys.find (qual.c_str ());
                if (m_iter != sm_OrgModKeys.end ()) {

                    COrgMod::ESubtype  mtype = m_iter->second;
                    if (x_AddQualifierToBioSrc (sfdata, mtype, val)) return true;

                }
            }
        }

    } else {

        TQualMap::const_iterator q_iter = sm_QualKeys.find (qual.c_str ());
        if (q_iter != sm_QualKeys.end ()) {
            EQual qtype = q_iter->second;
            switch (typ) {
                case CSeqFeatData::e_Gene:
                    if (x_AddQualifierToGene (sfdata, qtype, val)) return true;
                    break;
                case CSeqFeatData::e_Cdregion:
                    if (x_AddQualifierToCdregion (sfp, sfdata, qtype, val, container, line, seq_id)) return true;
                    break;
                case CSeqFeatData::e_Rna:
                    if (x_AddQualifierToRna (sfdata, qtype, val, container, line, seq_id)) return true;
                    break;
                case CSeqFeatData::e_Imp:
                    if (x_AddQualifierToImp (sfp, sfdata, qtype, qual, val)) return true;
                    break;
                case CSeqFeatData::e_Region:
                    if (qtype == eQual_region_name) {
                        sfdata.SetRegion (val);
                        return true;
                    }
                    break;
                case CSeqFeatData::e_Bond:
                    if (qtype == eQual_bond_type) {
                        CSeqFeatData::EBond btyp = CSeqFeatData::eBond_other;
                        if (CSeqFeatData::GetBondList()->IsBondName(val.c_str(), btyp)) {
                            sfdata.SetBond (btyp);
                            return true;
                        }
                    }
                    break;
                case CSeqFeatData::e_Site:
                    if (qtype == eQual_site_type) {
                        CSeqFeatData::ESite styp = CSeqFeatData::eSite_other;
                        if (CSeqFeatData::GetSiteList()->IsSiteName( val.c_str(), styp)) {
                            sfdata.SetSite (styp);
                            return true;
                        }
                    }
                    break;
                case CSeqFeatData::e_Pub:
                    if( qtype == eQual_PubMed ) {
                        CRef<CPub> new_pub( new CPub );
                        new_pub->SetPmid( CPubMedId( x_StringToLongNoThrow(val, container, seq_id, line, feat_name, qual) ) );
                        sfdata.SetPub().SetPub().Set().push_back( new_pub );
                        return true;
                    }
                    break;
                case CSeqFeatData::e_Prot:
                    switch( qtype ) {
                    case eQual_product:
                        sfdata.SetProt().SetName().push_back( val );
                        return true;
                    case eQual_function:
                        sfdata.SetProt().SetActivity().push_back( val );
                        return true;
                    case eQual_EC_number:
                        sfdata.SetProt().SetEc().push_back( val );
                        return true;
                    default:
                        break;
                    }
                    break;
                default:
                    break;
            }
            switch (qtype) {
                case eQual_pseudo:
                    sfp->SetPseudo (true);
                    return true;
                case eQual_partial:
                    sfp->SetPartial (true);
                    return true;
                case eQual_exception:
                    sfp->SetExcept (true);
                    sfp->SetExcept_text (val);
                    return true;
                case eQual_ribosomal_slippage:
                    sfp->SetExcept (true);
                    sfp->SetExcept_text (qual);
                    return true;
                case eQual_trans_splicing:
                    sfp->SetExcept (true);
                    sfp->SetExcept_text (qual);
                    return true;
                case eQual_evidence:
                    if (val == "experimental") {
                        sfp->SetExp_ev (CSeq_feat::eExp_ev_experimental);
                    } else if (val == "not_experimental" || val == "non_experimental" ||
                               val == "not-experimental" || val == "non-experimental") {
                        sfp->SetExp_ev (CSeq_feat::eExp_ev_not_experimental);
                    }
                    return true;
                case eQual_note:
                    {
                        if (sfp->CanGetComment ()) {
                            const CSeq_feat::TComment& comment = sfp->GetComment ();
                            CSeq_feat::TComment revised = comment + "; " + val;
                            sfp->SetComment (revised);
                        } else {
                            sfp->SetComment (val);
                        }
                        return true;
                    }
                case eQual_inference:
                    {
                        string prefix = "", remainder = "";
                        CInferencePrefixList::GetPrefixAndRemainder (val, prefix, remainder);
                        if (!NStr::IsBlank(prefix) && NStr::StartsWith (val, prefix)) {
                            x_AddGBQualToFeature (sfp, qual, val);
                            return true;
                        }
                        return false;
                    }
                case eQual_replace:
                    {
                        string val_copy = val;
                        NStr::ToLower( val_copy );
                        x_AddGBQualToFeature (sfp, qual, val_copy );
                        return true;
                    }
                case eQual_allele:
                case eQual_bound_moiety:
                case eQual_clone:
                case eQual_compare:
                case eQual_cons_splice:
                case eQual_direction:
                case eQual_EC_number:
                case eQual_experiment:
                case eQual_frequency:
                case eQual_function:
                case eQual_insertion_seq:
                case eQual_label:
                case eQual_map:
                case eQual_ncRNA_class:
                case eQual_number:
                case eQual_old_locus_tag:
                case eQual_operon:
                case eQual_organism:
                case eQual_PCR_conditions:
                case eQual_phenotype:
                case eQual_product:
                case eQual_protein_id:
                case eQual_satellite:
                case eQual_rpt_family:
                case eQual_rpt_type:
                case eQual_rpt_unit:
                case eQual_rpt_unit_range:
                case eQual_rpt_unit_seq:
                case eQual_standard_name:
                case eQual_tag_peptide:
                case eQual_transcript_id:
                case eQual_transposon:
                case eQual_usedin:
                    {
                        x_AddGBQualToFeature (sfp, qual, val);
                        return true;
                    }
                case eQual_gene:
                    {
                        CGene_ref& grp = sfp->SetGeneXref ();
                        if (val == "-") {
                            grp.SetLocus ("");
                        } else {
                            grp.SetLocus (val);
                        }
                        return true;
                    }
                case eQual_gene_desc:
                    {
                        CGene_ref& grp = sfp->SetGeneXref ();
                        grp.SetDesc (val);
                        return true;
                    }
                case eQual_gene_syn:
                    {
                        CGene_ref& grp = sfp->SetGeneXref ();
                        CGene_ref::TSyn& syn = grp.SetSyn ();
                        syn.push_back (val);
                        return true;
                    }
                case eQual_locus_tag:
                    {
                        CGene_ref& grp = sfp->SetGeneXref ();
                        grp.SetLocus_tag (val);
                        return true;
                    }
                case eQual_db_xref:
                    {
                        string db, tag;
                        int num;
                        if (NStr::SplitInTwo (val, ":", db, tag)) {
                            CSeq_feat::TDbxref& dblist = sfp->SetDbxref ();
                            CRef<CDbtag> dbt (new CDbtag);
                            dbt->SetDb (db);
                            CRef<CObject_id> oid (new CObject_id);
                            num = x_StringToLongNoThrow (tag, container, seq_id, line, feat_name, qual);
                            if (num != -1) {
                                oid->SetId (num);
                            } else {
                                oid->SetStr (tag);
                            }
                            dbt->SetTag (*oid);
                            dblist.push_back (dbt);
                            return true;
                        }
                        return true;
                    }
                case eQual_nomenclature:
                    {
                        /* !!! need to implement !!! */
                        return true;
                    }
                case eQual_go_component:
                case eQual_go_function:
                case eQual_go_process:
                    {
                        /*
                         CSeq_feat::TExt& ext = sfp->SetExt ();
                         CObject_id& obj = ext.SetType ();
                         if ((! obj.IsStr ()) || obj.GetStr ().empty ()) {
                             obj.SetStr ("GeneOntology");
                         }
                         (need more implementation here)
                         */
                         return true;
                    }
                default:
                    break;
            }
        }
    }
    return false;
}


bool CFeature_table_reader_imp::x_AddIntervalToFeature (
    CRef<CSeq_feat> sfp,
    CSeq_loc_mix& mix,
    const string& seqid,
    Int4 start,
    Int4 stop,
    bool partial5,
    bool partial3,
    bool ispoint,
    bool isminus
)

{
    CSeq_interval::TStrand strand = eNa_strand_plus;

    if (start > stop) {
        Int4 flip = start;
        start = stop;
        stop = flip;
        strand = eNa_strand_minus;
    }
    if (isminus) {
        strand = eNa_strand_minus;
    }

    if (ispoint) {
        // between two bases
        CRef<CSeq_loc> loc(new CSeq_loc);
        CSeq_point& point = loc->SetPnt ();
        point.SetPoint (start);
        point.SetStrand (strand);
        point.SetRightOf (true);
        CSeq_id seq_id (seqid);
        point.SetId().Assign (seq_id);
        mix.Set().push_back(loc);
    } else if (start == stop) {
        // just a point
        CRef<CSeq_loc> loc(new CSeq_loc);
        CSeq_point& point = loc->SetPnt ();
        point.SetPoint (start);
        point.SetStrand (strand);
        CSeq_id seq_id (seqid);
        point.SetId().Assign (seq_id);
        mix.Set().push_back (loc);
    } else {
        // interval
        CRef<CSeq_loc> loc(new CSeq_loc);
        CSeq_interval& ival = loc->SetInt ();
        ival.SetFrom (start);
        ival.SetTo (stop);
        ival.SetStrand (strand);
        if (partial5) {
            ival.SetPartialStart (true, eExtreme_Biological);
        }
        if (partial3) {
            ival.SetPartialStop (true, eExtreme_Biological);
        }
        CSeq_id seq_id (seqid);
        ival.SetId().Assign (seq_id);
        mix.Set().push_back (loc);
    }

    if (partial5 || partial3) {
        sfp->SetPartial (true);
    }

    return true;
}


bool CFeature_table_reader_imp::x_SetupSeqFeat (
    CRef<CSeq_feat> sfp,
    const string& feat,
    const CFeature_table_reader::TFlags flags,
    unsigned int line,
    const std::string &seq_id,
    IErrorContainer* container,
    ITableFilter *filter
)

{
    if (feat.empty ()) return false;

    // check filter, if any
    if( NULL != filter ) {
        ITableFilter::EResult result = filter->IsFeatureNameOkay(feat);
        if( result != ITableFilter::eResult_Okay ) {
            x_ProcessMsg( container, 
                ILineError::eProblem_FeatureNameNotAllowed,
                eDiag_Warning, seq_id, line, feat );
            if( result == ITableFilter::eResult_Disallowed ) {
                return false;
            }
        }
    }

    TFeatMap::const_iterator f_iter = sm_FeatKeys.find (feat.c_str ());
    if (f_iter != sm_FeatKeys.end ()) {

        CSeqFeatData::ESubtype sbtyp = f_iter->second;
        if (sbtyp != CSeqFeatData::eSubtype_bad) {

            // populate *sfp here...

            CSeqFeatData::E_Choice typ = CSeqFeatData::GetTypeFromSubtype (sbtyp);
            sfp->SetData ().Select (typ);
            CSeqFeatData& sfdata = sfp->SetData ();
    
            if (typ == CSeqFeatData::e_Rna) {
                CRNA_ref& rrp = sfdata.SetRna ();
                CRNA_ref::EType rnatyp = CRNA_ref::eType_unknown;
                switch (sbtyp) {
                    case CSeqFeatData::eSubtype_preRNA :
                        rnatyp = CRNA_ref::eType_premsg;
                        break;
                    case CSeqFeatData::eSubtype_mRNA :
                        rnatyp = CRNA_ref::eType_mRNA;
                        break;
                    case CSeqFeatData::eSubtype_tRNA :
                        rnatyp = CRNA_ref::eType_tRNA;
                        break;
                    case CSeqFeatData::eSubtype_rRNA :
                        rnatyp = CRNA_ref::eType_rRNA;
                        break;
                    case CSeqFeatData::eSubtype_snRNA :
                        rrp.SetExt().SetName("ncRNA");
                        rnatyp = CRNA_ref::eType_other;
                        x_AddGBQualToFeature (sfp, "ncRNA_class", "snRNA");
                        break;
                    case CSeqFeatData::eSubtype_scRNA :
                        rrp.SetExt().SetName("ncRNA");
                        rnatyp = CRNA_ref::eType_other;
                        x_AddGBQualToFeature (sfp, "ncRNA_class", "scRNA");
                        break;
                    case CSeqFeatData::eSubtype_snoRNA :
                        rrp.SetExt().SetName("ncRNA");
                        rnatyp = CRNA_ref::eType_other;
                        x_AddGBQualToFeature (sfp, "ncRNA_class", "snoRNA");
                        break;
                    case CSeqFeatData::eSubtype_ncRNA :
                        rrp.SetExt().SetName("ncRNA");
                        rnatyp = CRNA_ref::eType_other;
                        break;
                    case CSeqFeatData::eSubtype_tmRNA :
                        rrp.SetExt().SetName("tmRNA");
                        rnatyp = CRNA_ref::eType_other;
                        break;
                    case CSeqFeatData::eSubtype_otherRNA :
                        rrp.SetExt().SetName("misc_RNA");
                        rnatyp = CRNA_ref::eType_other;
                        break;
                    default :
                        break;
                }
                rrp.SetType (rnatyp);
   
            } else if (typ == CSeqFeatData::e_Imp) {
                CImp_feat_Base& imp = sfdata.SetImp ();
                imp.SetKey (feat);
    
            } else if (typ == CSeqFeatData::e_Bond) {
                sfdata.SetBond (CSeqFeatData::eBond_other);
                
            } else if (typ == CSeqFeatData::e_Site) {
                sfdata.SetSite (CSeqFeatData::eSite_other);
            } else if (typ == CSeqFeatData::e_Prot ) {
                CProt_ref &prot_ref = sfdata.SetProt();
                if( sbtyp == CSeqFeatData::eSubtype_mat_peptide_aa ) {
                    prot_ref.SetProcessed( CProt_ref::eProcessed_mature );
                }
            }

            return true;
        }
    }

    // unrecognized feature key

    if ((flags & CFeature_table_reader::fReportBadKey) != 0) {
        x_ProcessMsg(container, ILineError::eProblem_UnrecognizedFeatureName, eDiag_Warning, seq_id, line, feat );
    }

    if ((flags & CFeature_table_reader::fTranslateBadKey) != 0) {

        sfp->SetData ().Select (CSeqFeatData::e_Imp);
        CSeqFeatData& sfdata = sfp->SetData ();
        CImp_feat_Base& imp = sfdata.SetImp ();
        imp.SetKey ("misc_feature");
        x_AddQualifierToFeature (sfp, kEmptyStr, "standard_name", feat, container, line, seq_id);

        return true;

    } else if ((flags & CFeature_table_reader::fKeepBadKey) != 0) {

        sfp->SetData ().Select (CSeqFeatData::e_Imp);
        CSeqFeatData& sfdata = sfp->SetData ();
        CImp_feat_Base& imp = sfdata.SetImp ();
        imp.SetKey (feat);

        return true;
    }

    return false;
}


void CFeature_table_reader_imp::x_ProcessMsg(
    IErrorContainer* container,
    ILineError::EProblem eProblem,
    EDiagSev eSeverity,
    const std::string& strSeqId,
    unsigned int uLine,
    const std::string & strFeatureName,
    const std::string & strQualifierName,
    const std::string & strQualifierValue )
{
    CLineError err( eProblem, eSeverity, strSeqId, uLine, 
        strFeatureName, strQualifierName, strQualifierValue );
    if (container == 0) {
        throw (err);
    }

    if ( !container->PutError(err) ) {
        throw(err);
    }
}
                                             

CRef<CSeq_annot> CFeature_table_reader_imp::ReadSequinFeatureTable (
    ILineReader& reader,
    const string& seqid,
    const string& annotname,
    const CFeature_table_reader::TFlags flags,
    IErrorContainer* container,
    ITableFilter *filter
)
{
    string feat, qual, val;
    string curr_feat_name;
    Int4 start, stop;
    bool partial5, partial3, ispoint, isminus, ignore_until_next_feature_key = false;
    Int4 offset = 0;
    CRef<CSeq_annot> sap(new CSeq_annot);
    CSeq_annot::C_Data::TFtable& ftable = sap->SetData().SetFtable();

    // Use this to efficiently find the best CDS for a prot feature
    // (only add CDS's for it to work right)
    CBestFeatFinder best_CDS_finder;

    CRef<CSeq_feat> sfp;

    if (! annotname.empty ()) {
      CAnnot_descr& descr = sap->SetDesc ();
      CRef<CAnnotdesc> annot(new CAnnotdesc);
      annot->SetName (annotname);
      descr.Set().push_back (annot);
    }

    while ( !reader.AtEOF() ) {

        CTempString line = *++reader;

        if (! line.empty ()) {
            if( s_IsFeatureLineAndFix(line) ) {
                // if next feature table, return current sap
                reader.UngetLine(); // we'll get this feature line the next time around
                return sap;
            } if (line [0] == '[') {

                // set offset !!!!!!!!

            } else if ( s_LineIndicatesOrder(line) ) {

                // put nulls between feature intervals
                CRef<CSeq_loc> loc_with_nulls = s_LocationJoinToOrder( sfp->GetLocation() );
                // loc_with_nulls is unset if no change was needed
                if( loc_with_nulls ) {
                    sfp->SetLocation( *loc_with_nulls );
                }

            } else if (x_ParseFeatureTableLine (line, &start, &stop, &partial5, &partial3,
                                                &ispoint, &isminus, feat, qual, val, offset,
                                                container, reader.GetLineNumber(), seqid)) {

                // process line in feature table

                replace( val.begin(), val.end(), '\"', '\'' );

                if ((! feat.empty ()) && start >= 0 && stop >= 0) {

                    // process start - stop - feature line

                    sfp.Reset (new CSeq_feat);
                    sfp->ResetLocation ();

                    if (x_SetupSeqFeat (sfp, feat, flags, reader.GetLineNumber(), seqid, container, filter)) {

                        ftable.push_back (sfp);

                        // now create location

                        CRef<CSeq_loc> location (new CSeq_loc);
                        sfp->SetLocation (*location);

                        // if new feature is a CDS, remember it for later lookups
                        if( sfp->CanGetData() && sfp->GetData().IsCdregion() ) {
                            best_CDS_finder.AddFeat( *sfp );
                        }

                        // and add first interval
                        x_AddIntervalToFeature (sfp, location->SetMix(), 
                                                seqid, start, stop, partial5, partial3, ispoint, isminus);

                        ignore_until_next_feature_key = false;

                        curr_feat_name = feat;

                    } else {

                        // bad feature, set ignore flag

                        ignore_until_next_feature_key = true;
                    }

                } else if (ignore_until_next_feature_key) {

                    // bad feature, ignore qualifiers until next feature key

                } else if (start >= 0 && stop >= 0 && feat.empty () && qual.empty () && val.empty ()) {

                    // process start - stop multiple interval line

                    if (sfp  &&  sfp->IsSetLocation()  &&  sfp->GetLocation().IsMix()) {
                        x_AddIntervalToFeature (sfp, sfp->SetLocation().SetMix(), 
                                                seqid, start, stop, partial5, partial3, ispoint, isminus);
                    } else {
                        if ((flags & CFeature_table_reader::fReportBadKey) != 0) {
                            x_ProcessMsg(container, ILineError::eProblem_NoFeatureProvidedOnIntervals,
                                eDiag_Warning,
                                seqid,
                                reader.GetLineNumber() );
                        }
                    }

                } else if ((! qual.empty ()) && (! val.empty ())) {

                    // process qual - val qualifier line

                    if ( !sfp ) {
                        if ((flags & CFeature_table_reader::fReportBadKey) != 0) {
                            x_ProcessMsg(container, 
                                ILineError::eProblem_QualifierWithoutFeature, 
                                eDiag_Warning,
                                seqid,
                                reader.GetLineNumber(), kEmptyStr, qual, val );
                        }
                    } else if ( !x_AddQualifierToFeature (sfp, curr_feat_name, qual, val, container, reader.GetLineNumber(), seqid) ) {

                        // unrecognized qualifier key

                        if ((flags & CFeature_table_reader::fReportBadKey) != 0) {
                            x_ProcessMsg(container,
                                ILineError::eProblem_UnrecognizedQualifierName, 
                                eDiag_Warning, seqid, reader.GetLineNumber(), curr_feat_name, qual, val );
                        }

                        if ((flags & CFeature_table_reader::fKeepBadKey) != 0) {
                            x_AddGBQualToFeature (sfp, qual, val);
                        }
                    }

                } else if ((! qual.empty ()) && (val.empty ())) {

                    // check for the few qualifiers that do not need a value
                    if ( !sfp ) {
                        if ((flags & CFeature_table_reader::fReportBadKey) != 0) {
                            x_ProcessMsg(container, 
                                ILineError::eProblem_QualifierWithoutFeature, eDiag_Warning,
                                seqid, reader.GetLineNumber(),
                                kEmptyStr, qual );
                        }
                    } else {
                        TSingleSet::const_iterator s_iter = sc_SingleKeys.find (qual.c_str ());
                        if (s_iter != sc_SingleKeys.end ()) {

                            x_AddQualifierToFeature (sfp, curr_feat_name, qual, val, container, reader.GetLineNumber(), seqid);
                        }
                    }
                } else if (! feat.empty ()) {
                
                    // unrecognized location

                    if ((flags & CFeature_table_reader::fReportBadKey) != 0) {
                        x_ProcessMsg( container, 
                            ILineError::eProblem_FeatureBadStartAndOrStop, eDiag_Warning,
                            seqid, reader.GetLineNumber(),
                            feat );
                    }
                }
            }
        }
    }

    return sap;
}


CRef<CSeq_feat> CFeature_table_reader_imp::CreateSeqFeat (
    const string& feat,
    CSeq_loc& location,
    const CFeature_table_reader::TFlags flags,
    IErrorContainer* container,
    unsigned int line,
    const string &seq_id,
    ITableFilter *filter
)

{
    CRef<CSeq_feat> sfp (new CSeq_feat);

    sfp->ResetLocation ();

    if ( ! x_SetupSeqFeat (sfp, feat, flags, line, seq_id, container, filter) ) {

        // bad feature, make dummy

        sfp->SetData ().Select (CSeqFeatData::e_not_set);
        /*
        sfp->SetData ().Select (CSeqFeatData::e_Imp);
        CSeqFeatData& sfdata = sfp->SetData ();
        CImp_feat_Base& imp = sfdata.SetImp ();
        imp.SetKey ("bad_feature");
        */
    }
 
    sfp->SetLocation (location);

    return sfp;
}


void CFeature_table_reader_imp::AddFeatQual (
    CRef<CSeq_feat> sfp,
    const string& feat_name,
    const string& qual,
    const string& val,
    const CFeature_table_reader::TFlags flags,
    IErrorContainer* container,
    int line, 	
    const string &seq_id )

{
    if ((! qual.empty ()) && (! val.empty ())) {

        if (! x_AddQualifierToFeature (sfp, feat_name, qual, val, container, line, seq_id)) {

            // unrecognized qualifier key

            if ((flags & CFeature_table_reader::fReportBadKey) != 0) {
                ERR_POST_X (5, Warning << "Unrecognized qualifier '" << qual << "'");
            }

            if ((flags & CFeature_table_reader::fKeepBadKey) != 0) {
                x_AddGBQualToFeature (sfp, qual, val);
            }
        }

    } else if ((! qual.empty ()) && (val.empty ())) {

        // check for the few qualifiers that do not need a value

        TSingleSet::const_iterator s_iter = sc_SingleKeys.find (qual.c_str ());
        if (s_iter != sc_SingleKeys.end ()) {

            x_AddQualifierToFeature (sfp, feat_name, qual, val, container, line, seq_id);

        }
    }
}


// public access functions

CRef<CSeq_annot> CFeature_table_reader::ReadSequinFeatureTable (
    CNcbiIstream& ifs,
    const string& seqid,
    const string& annotname,
    const TFlags flags,
    IErrorContainer* container,
    ITableFilter *filter
)
{
    CStreamLineReader reader(ifs);
    return ReadSequinFeatureTable(reader, seqid, annotname, flags, container, filter);
}


CRef<CSeq_annot> CFeature_table_reader::ReadSequinFeatureTable (
    ILineReader& reader,
    const string& seqid,
    const string& annotname,
    const TFlags flags,
    IErrorContainer* container,
    ITableFilter *filter
)
{
    // just read features from 5-column table

    CRef<CSeq_annot> sap = x_GetImplementation().ReadSequinFeatureTable 
      (reader, seqid, annotname, flags, container, filter);

    // go through all features and demote single interval seqlocmix to seqlocint
    for (CTypeIterator<CSeq_feat> fi(*sap); fi; ++fi) {
        CSeq_feat& feat = *fi;
        CSeq_loc& location = feat.SetLocation ();
        if (location.IsMix ()) {
            CSeq_loc_mix& mx = location.SetMix ();
            CSeq_loc &keep_loc(*mx.Set ().front ());
            CRef<CSeq_loc> guard_loc(&keep_loc);            
            switch (mx.Get ().size ()) {
                case 0:
                    location.SetNull ();
                    break;
                case 1:
                    feat.SetLocation (*mx.Set ().front ());
                    break;
                default:
                    break;
            }
        }
    }

    return sap;
}


CRef<CSeq_annot> CFeature_table_reader::ReadSequinFeatureTable (
    CNcbiIstream& ifs,
    const TFlags flags,
    IErrorContainer* container,
    ITableFilter *filter
)
{
    CStreamLineReader reader(ifs);
    return ReadSequinFeatureTable(reader, flags, container, filter);
}


CRef<CSeq_annot> CFeature_table_reader::ReadSequinFeatureTable (
    ILineReader& reader,
    const TFlags flags,
    IErrorContainer* container,
    ITableFilter *filter
)
{
    string fst, scd, seqid, annotname;

    // first look for >Feature line, extract seqid and optional annotname
    while (seqid.empty () && !reader.AtEOF() ) {

        CTempString line = *++reader;

        if (! line.empty ()) {
            if ( s_IsFeatureLineAndFix(line) ) {
                if (NStr::StartsWith (line, ">Feature")) {
                    NStr::SplitInTwo (line, " ", fst, scd);
                    NStr::SplitInTwo (scd, " ", seqid, annotname);
                }
            }
        }
    }

    // then read features from 5-column table
    return ReadSequinFeatureTable (reader, seqid, annotname, flags, container, filter);

}


void CFeature_table_reader::ReadSequinFeatureTables(
    CNcbiIstream& ifs,
    CSeq_entry& entry,
    const TFlags flags,
    IErrorContainer* container,
    ITableFilter *filter
)
{
    CStreamLineReader reader(ifs);
    return ReadSequinFeatureTables(reader, entry, flags, container, filter);
}


void CFeature_table_reader::ReadSequinFeatureTables(
    ILineReader& reader,
    CSeq_entry& entry,
    const TFlags flags,
    IErrorContainer* container,
    ITableFilter *filter
)
{
    while ( !reader.AtEOF() ) {
        CRef<CSeq_annot> annot = ReadSequinFeatureTable(reader, flags, container, filter);
        if (entry.IsSeq()) { // only one place to go
            entry.SetSeq().SetAnnot().push_back(annot);
            continue;
        }
        _ASSERT(annot->GetData().IsFtable());
        if (annot->GetData().GetFtable().empty()) {
            continue;
        }
        // otherwise, take the first feature, which should be representative
        const CSeq_feat& feat    = *annot->GetData().GetFtable().front();
        const CSeq_id*   feat_id = feat.GetLocation().GetId();
        CBioseq*         seq     = NULL;
        _ASSERT(feat_id); // we expect a uniform sequence ID
        for (CTypeIterator<CBioseq> seqit(entry);  seqit  &&  !seq;  ++seqit) {
            ITERATE (CBioseq::TId, seq_id, seqit->GetId()) {
                if (feat_id->Match(**seq_id)) {
                    seq = &*seqit;
                    break;
                }
            }
        }
        if (seq) { // found a match
            seq->SetAnnot().push_back(annot);
        } else { // just package on the set
            ERR_POST_X(6, Warning
                       << "ReadSequinFeatureTables: unable to find match for "
                       << feat_id->AsFastaString());
            entry.SetSet().SetAnnot().push_back(annot);
        }
    }
}


CRef<CSeq_feat> CFeature_table_reader::CreateSeqFeat (
    const string& feat,
    CSeq_loc& location,
    const TFlags flags,
    IErrorContainer* container,
    unsigned int line,
    string *seq_id,
    ITableFilter *filter
)

{
    return x_GetImplementation ().CreateSeqFeat (feat, location, flags, container, line, 
        (seq_id ? *seq_id : string() ), filter);
}


void CFeature_table_reader::AddFeatQual (
    CRef<CSeq_feat> sfp,
    const string& feat_name,
    const string& qual,
    const string& val,
    const CFeature_table_reader::TFlags flags,
    IErrorContainer* container,
    int line, 	
    const string &seq_id 
)

{
    x_GetImplementation ().AddFeatQual ( sfp, feat_name, qual, val, flags, container, line, seq_id ) ;
}


END_objects_SCOPE
END_NCBI_SCOPE
