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
#include <objects/seqfeat/SeqFeatData.hpp>

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

#include <objtools/readers/readfeat.hpp>

#include <algorithm>

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
        eQual_frequency,
        eQual_function,
        eQual_gene,
        eQual_gene_desc,
        eQual_gene_syn,
        eQual_go_component,
        eQual_go_function,
        eQual_go_process,
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
        eQual_rpt_family,
        eQual_rpt_type,
        eQual_rpt_unit,
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
        eQual_transcript_id,
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
    CRef<CSeq_annot> ReadSequinFeatureTable (CNcbiIstream& ifs,
                                             const string& seqid,
                                             const string& annotname,
                                             const CFeature_table_reader::TFlags flags);

    // create single feature from key
    CRef<CSeq_feat> CreateSeqFeat (const string& feat,
                                   CSeq_loc& location,
                                   const CFeature_table_reader::TFlags flags);

    // add single qualifier to feature
    void AddFeatQual (CRef<CSeq_feat> sfp,
                      const string& qual,
                      const string& val,
                      const CFeature_table_reader::TFlags flags);

private:
    // Prohibit copy constructor and assignment operator
    CFeature_table_reader_imp(const CFeature_table_reader_imp& value);
    CFeature_table_reader_imp& operator=(const CFeature_table_reader_imp& value);

    bool x_ParseFeatureTableLine (const string& line, Int4* startP, Int4* stopP,
                                  bool* partial5P, bool* partial3P, bool* ispointP,
                                  string& featP, string& qualP, string& valP, Int4 offset);

    bool x_AddIntervalToFeature (CRef<CSeq_feat> sfp, CSeq_loc_mix *mix,
                                 const string& seqid, Int4 start, Int4 stop,
                                 bool partial5, bool partial3, bool ispoint);

    bool x_AddQualifierToFeature (CRef<CSeq_feat> sfp,
                                  const string& qual, const string& val);

    bool x_AddQualifierToGene     (CSeqFeatData& sfdata,
                                   EQual qtype, const string& val);
    bool x_AddQualifierToCdregion (CRef<CSeq_feat> sfp, CSeqFeatData& sfdata,
                                   EQual qtype, const string& val);
    bool x_AddQualifierToRna      (CSeqFeatData& sfdata,
                                   EQual qtype, const string& val);
    bool x_AddQualifierToImp      (CRef<CSeq_feat> sfp, CSeqFeatData& sfdata,
                                   EQual qtype, const string& qual, const string& val);
    bool x_AddQualifierToBioSrc   (CSeqFeatData& sfdata,
                                   EOrgRef rtype, const string& val);
    bool x_AddQualifierToBioSrc   (CSeqFeatData& sfdata,
                                   CSubSource::ESubtype stype, const string& val);
    bool x_AddQualifierToBioSrc   (CSeqFeatData& sfdata,
                                   COrgMod::ESubtype mtype, const string& val);

    bool x_StringIsJustQuotes (const string& str);

    int x_ParseTrnaString (const string& val);
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
    TFeatKey ( "3'clip",             CSeqFeatData::eSubtype_3clip              ),
    TFeatKey ( "3'UTR",              CSeqFeatData::eSubtype_3UTR               ),
    TFeatKey ( "5'clip",             CSeqFeatData::eSubtype_5clip              ),
    TFeatKey ( "5'UTR",              CSeqFeatData::eSubtype_5UTR               ),
    TFeatKey ( "attenuator",         CSeqFeatData::eSubtype_attenuator         ),
    TFeatKey ( "Bond",               CSeqFeatData::eSubtype_bond               ),
    TFeatKey ( "C_region",           CSeqFeatData::eSubtype_C_region           ),
    TFeatKey ( "CAAT_signal",        CSeqFeatData::eSubtype_CAAT_signal        ),
    TFeatKey ( "CDS",                CSeqFeatData::eSubtype_cdregion           ),
    TFeatKey ( "Cit",                CSeqFeatData::eSubtype_pub                ),
    TFeatKey ( "Comment",            CSeqFeatData::eSubtype_comment            ),
    TFeatKey ( "conflict",           CSeqFeatData::eSubtype_conflict           ),
    TFeatKey ( "D-loop",             CSeqFeatData::eSubtype_D_loop             ),
    TFeatKey ( "D_segment",          CSeqFeatData::eSubtype_D_segment          ),
    TFeatKey ( "enhancer",           CSeqFeatData::eSubtype_enhancer           ),
    TFeatKey ( "exon",               CSeqFeatData::eSubtype_exon               ),
    TFeatKey ( "GC_signal",          CSeqFeatData::eSubtype_GC_signal          ),
    TFeatKey ( "gene",               CSeqFeatData::eSubtype_gene               ),
    TFeatKey ( "Het",                CSeqFeatData::eSubtype_het                ),
    TFeatKey ( "iDNA",               CSeqFeatData::eSubtype_iDNA               ),
    TFeatKey ( "intron",             CSeqFeatData::eSubtype_intron             ),
    TFeatKey ( "J_segment",          CSeqFeatData::eSubtype_J_segment          ),
    TFeatKey ( "LTR",                CSeqFeatData::eSubtype_LTR                ),
    TFeatKey ( "mat_peptide",        CSeqFeatData::eSubtype_mat_peptide_aa     ),
    TFeatKey ( "mat_peptide_nt",     CSeqFeatData::eSubtype_mat_peptide        ),
    TFeatKey ( "misc_binding",       CSeqFeatData::eSubtype_misc_binding       ),
    TFeatKey ( "misc_difference",    CSeqFeatData::eSubtype_misc_difference    ),
    TFeatKey ( "misc_feature",       CSeqFeatData::eSubtype_misc_feature       ),
    TFeatKey ( "misc_recomb",        CSeqFeatData::eSubtype_misc_recomb        ),
    TFeatKey ( "misc_RNA",           CSeqFeatData::eSubtype_otherRNA           ),
    TFeatKey ( "misc_signal",        CSeqFeatData::eSubtype_misc_signal        ),
    TFeatKey ( "misc_structure",     CSeqFeatData::eSubtype_misc_structure     ),
    TFeatKey ( "modified_base",      CSeqFeatData::eSubtype_modified_base      ),
    TFeatKey ( "mRNA",               CSeqFeatData::eSubtype_mRNA               ),
    TFeatKey ( "N_region",           CSeqFeatData::eSubtype_N_region           ),
    TFeatKey ( "NonStdRes",          CSeqFeatData::eSubtype_non_std_residue    ),
    TFeatKey ( "Num",                CSeqFeatData::eSubtype_num                ),
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
    TFeatKey ( "Protein",            CSeqFeatData::eSubtype_prot               ),
    TFeatKey ( "protein_bind",       CSeqFeatData::eSubtype_protein_bind       ),
    TFeatKey ( "RBS",                CSeqFeatData::eSubtype_RBS                ),
    TFeatKey ( "REFERENCE",          CSeqFeatData::eSubtype_pub                ),
    TFeatKey ( "Region",             CSeqFeatData::eSubtype_region             ),
    TFeatKey ( "rep_origin",         CSeqFeatData::eSubtype_rep_origin         ),
    TFeatKey ( "repeat_region",      CSeqFeatData::eSubtype_repeat_region      ),
    TFeatKey ( "repeat_unit",        CSeqFeatData::eSubtype_repeat_unit        ),
    TFeatKey ( "rRNA",               CSeqFeatData::eSubtype_rRNA               ),
    TFeatKey ( "Rsite",              CSeqFeatData::eSubtype_rsite              ),
    TFeatKey ( "S_region",           CSeqFeatData::eSubtype_S_region           ),
    TFeatKey ( "satellite",          CSeqFeatData::eSubtype_satellite          ),
    TFeatKey ( "scRNA",              CSeqFeatData::eSubtype_scRNA              ),
    TFeatKey ( "SecStr",             CSeqFeatData::eSubtype_psec_str           ),
    TFeatKey ( "sig_peptide",        CSeqFeatData::eSubtype_sig_peptide_aa     ),
    TFeatKey ( "sig_peptide_nt",     CSeqFeatData::eSubtype_sig_peptide        ),
    TFeatKey ( "Site",               CSeqFeatData::eSubtype_site               ),
    TFeatKey ( "Site-ref",           CSeqFeatData::eSubtype_site_ref           ),
    TFeatKey ( "snoRNA",             CSeqFeatData::eSubtype_snoRNA             ),
    TFeatKey ( "snRNA",              CSeqFeatData::eSubtype_snRNA              ),
    TFeatKey ( "source",             CSeqFeatData::eSubtype_biosrc             ),
    TFeatKey ( "Src",                CSeqFeatData::eSubtype_biosrc             ),
    TFeatKey ( "stem_loop",          CSeqFeatData::eSubtype_stem_loop          ),
    TFeatKey ( "STS",                CSeqFeatData::eSubtype_STS                ),
    TFeatKey ( "TATA_signal",        CSeqFeatData::eSubtype_TATA_signal        ),
    TFeatKey ( "terminator",         CSeqFeatData::eSubtype_terminator         ),
    TFeatKey ( "transit_peptide",    CSeqFeatData::eSubtype_transit_peptide_aa ),
    TFeatKey ( "transit_peptide_nt", CSeqFeatData::eSubtype_transit_peptide    ),
    TFeatKey ( "tRNA",               CSeqFeatData::eSubtype_tRNA               ),
    TFeatKey ( "TxInit",             CSeqFeatData::eSubtype_txinit             ),
    TFeatKey ( "unsure",             CSeqFeatData::eSubtype_unsure             ),
    TFeatKey ( "User",               CSeqFeatData::eSubtype_user               ),
    TFeatKey ( "V_region",           CSeqFeatData::eSubtype_V_region           ),
    TFeatKey ( "V_segment",          CSeqFeatData::eSubtype_V_segment          ),
    TFeatKey ( "variation",          CSeqFeatData::eSubtype_variation          ),
    TFeatKey ( "virion",             CSeqFeatData::eSubtype_virion             ),
    TFeatKey ( "Xref",               CSeqFeatData::eSubtype_seq                )
};

typedef CStaticArrayMap <const char*, const CSeqFeatData::ESubtype, PNocase> TFeatMap;
static const TFeatMap sm_FeatKeys (feat_key_to_subtype, sizeof (feat_key_to_subtype));


typedef pair <const char *, const CFeature_table_reader_imp::EQual> TQualKey;

static const TQualKey qual_key_to_subtype [] = {
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
    TQualKey ( "EC_number",            CFeature_table_reader_imp::eQual_EC_number            ),
    TQualKey ( "evidence",             CFeature_table_reader_imp::eQual_evidence             ),
    TQualKey ( "exception",            CFeature_table_reader_imp::eQual_exception            ),
    TQualKey ( "frequency",            CFeature_table_reader_imp::eQual_frequency            ),
    TQualKey ( "function",             CFeature_table_reader_imp::eQual_function             ),
    TQualKey ( "gene",                 CFeature_table_reader_imp::eQual_gene                 ),
    TQualKey ( "gene_desc",            CFeature_table_reader_imp::eQual_gene_desc            ),
    TQualKey ( "gene_syn",             CFeature_table_reader_imp::eQual_gene_syn             ),
    TQualKey ( "go_component",         CFeature_table_reader_imp::eQual_go_component         ),
    TQualKey ( "go_function",          CFeature_table_reader_imp::eQual_go_function          ),
    TQualKey ( "go_process",           CFeature_table_reader_imp::eQual_go_process           ),
    TQualKey ( "insertion_seq",        CFeature_table_reader_imp::eQual_insertion_seq        ),
    TQualKey ( "label",                CFeature_table_reader_imp::eQual_label                ),
    TQualKey ( "loccnt",               CFeature_table_reader_imp::eQual_loccnt               ),
    TQualKey ( "locus_tag",            CFeature_table_reader_imp::eQual_locus_tag            ),
    TQualKey ( "macronuclear",         CFeature_table_reader_imp::eQual_macronuclear         ),
    TQualKey ( "map",                  CFeature_table_reader_imp::eQual_map                  ),
    TQualKey ( "method",               CFeature_table_reader_imp::eQual_method               ),
    TQualKey ( "mod_base",             CFeature_table_reader_imp::eQual_mod_base             ),
    TQualKey ( "note",                 CFeature_table_reader_imp::eQual_note                 ),
    TQualKey ( "number",               CFeature_table_reader_imp::eQual_number               ),
    TQualKey ( "old_locus_tag",        CFeature_table_reader_imp::eQual_old_locus_tag        ),
    TQualKey ( "operon",               CFeature_table_reader_imp::eQual_operon               ),
    TQualKey ( "organism",             CFeature_table_reader_imp::eQual_organism             ),
    TQualKey ( "partial",              CFeature_table_reader_imp::eQual_partial              ),
    TQualKey ( "PCR_conditions",       CFeature_table_reader_imp::eQual_PCR_conditions       ),
    TQualKey ( "phenotype",            CFeature_table_reader_imp::eQual_phenotype            ),
    TQualKey ( "product",              CFeature_table_reader_imp::eQual_product              ),
    TQualKey ( "prot_desc",            CFeature_table_reader_imp::eQual_prot_desc            ),
    TQualKey ( "protein_id",           CFeature_table_reader_imp::eQual_protein_id           ),
    TQualKey ( "pseudo",               CFeature_table_reader_imp::eQual_pseudo               ),
    TQualKey ( "replace",              CFeature_table_reader_imp::eQual_replace              ),
    TQualKey ( "rpt_family",           CFeature_table_reader_imp::eQual_rpt_family           ),
    TQualKey ( "rpt_type",             CFeature_table_reader_imp::eQual_rpt_type             ),
    TQualKey ( "rpt_unit",             CFeature_table_reader_imp::eQual_rpt_unit             ),
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
    TQualKey ( "STS",                  CFeature_table_reader_imp::eQual_STS                  ),
    TQualKey ( "sts_aliases",          CFeature_table_reader_imp::eQual_sts_aliases          ),
    TQualKey ( "sts_dsegs",            CFeature_table_reader_imp::eQual_sts_dsegs            ),
    TQualKey ( "transcript_id",        CFeature_table_reader_imp::eQual_transcript_id        ),
    TQualKey ( "transl_except",        CFeature_table_reader_imp::eQual_transl_except        ),
    TQualKey ( "transl_table",         CFeature_table_reader_imp::eQual_transl_table         ),
    TQualKey ( "translation",          CFeature_table_reader_imp::eQual_translation          ),
    TQualKey ( "transposon",           CFeature_table_reader_imp::eQual_transposon           ),
    TQualKey ( "usedin",               CFeature_table_reader_imp::eQual_usedin               ),
    TQualKey ( "weight",               CFeature_table_reader_imp::eQual_weight               )
};

typedef CStaticArrayMap <const char*, const CFeature_table_reader_imp::EQual, PNocase> TQualMap;
static const TQualMap sm_QualKeys (qual_key_to_subtype, sizeof (qual_key_to_subtype));


typedef pair <const char *, const CFeature_table_reader_imp::EOrgRef> TOrgRefKey;

static const TOrgRefKey orgref_key_to_subtype [] = {
    TOrgRefKey ( "div",        CFeature_table_reader_imp::eOrgRef_div       ),
    TOrgRefKey ( "gcode",      CFeature_table_reader_imp::eOrgRef_gcode     ),
    TOrgRefKey ( "lineage",    CFeature_table_reader_imp::eOrgRef_lineage   ),
    TOrgRefKey ( "mgcode",     CFeature_table_reader_imp::eOrgRef_mgcode    ),
    TOrgRefKey ( "organelle",  CFeature_table_reader_imp::eOrgRef_organelle ),
    TOrgRefKey ( "organism",   CFeature_table_reader_imp::eOrgRef_organism  )
};

typedef CStaticArrayMap <const char*, const CFeature_table_reader_imp::EOrgRef, PNocase> TOrgRefMap;
static const TOrgRefMap sm_OrgRefKeys (orgref_key_to_subtype, sizeof (orgref_key_to_subtype));


typedef pair <const char *, const CBioSource::EGenome> TGenomeKey;

static const TGenomeKey genome_key_to_subtype [] = {
    TGenomeKey ( "apicoplast",                CBioSource::eGenome_apicoplast       ),
    TGenomeKey ( "chloroplast",               CBioSource::eGenome_chloroplast      ),
    TGenomeKey ( "chromoplast",               CBioSource::eGenome_chromoplast      ),
    TGenomeKey ( "cyanelle",                  CBioSource::eGenome_cyanelle         ),
    TGenomeKey ( "endogenous_virus",          CBioSource::eGenome_endogenous_virus ),
    TGenomeKey ( "extrachrom",                CBioSource::eGenome_extrachrom       ),
    TGenomeKey ( "genomic",                   CBioSource::eGenome_genomic          ),
    TGenomeKey ( "insertion_seq",             CBioSource::eGenome_insertion_seq    ),
    TGenomeKey ( "kinetoplast",               CBioSource::eGenome_kinetoplast      ),
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
    TGenomeKey ( "proviral",                  CBioSource::eGenome_proviral         ),
    TGenomeKey ( "transposon",                CBioSource::eGenome_transposon       ),
    TGenomeKey ( "unknown",                   CBioSource::eGenome_unknown          ),
    TGenomeKey ( "virion",                    CBioSource::eGenome_virion           )
};

typedef CStaticArrayMap <const char*, const CBioSource::EGenome, PNocase> TGenomeMap;
static const TGenomeMap sm_GenomeKeys (genome_key_to_subtype, sizeof (genome_key_to_subtype));


typedef pair <const char *, const CSubSource::ESubtype> TSubSrcKey;

static const TSubSrcKey subsrc_key_to_subtype [] = {
    TSubSrcKey ( "cell_line",            CSubSource::eSubtype_cell_line             ),
    TSubSrcKey ( "cell_type",            CSubSource::eSubtype_cell_type             ),
    TSubSrcKey ( "chromosome",           CSubSource::eSubtype_chromosome            ),
    TSubSrcKey ( "clone",                CSubSource::eSubtype_clone                 ),
    TSubSrcKey ( "clone_lib",            CSubSource::eSubtype_clone_lib             ),
    TSubSrcKey ( "country",              CSubSource::eSubtype_country               ),
    TSubSrcKey ( "dev_stage",            CSubSource::eSubtype_dev_stage             ),
    TSubSrcKey ( "endogenous_virus",     CSubSource::eSubtype_endogenous_virus_name ),
    TSubSrcKey ( "environmental_sample", CSubSource::eSubtype_environmental_sample  ),
    TSubSrcKey ( "frequency",            CSubSource::eSubtype_frequency             ),
    TSubSrcKey ( "genotype",             CSubSource::eSubtype_genotype              ),
    TSubSrcKey ( "germline",             CSubSource::eSubtype_germline              ),
    TSubSrcKey ( "haplotype",            CSubSource::eSubtype_haplotype             ),
    TSubSrcKey ( "insertion_seq",        CSubSource::eSubtype_insertion_seq_name    ),
    TSubSrcKey ( "isolation_source",     CSubSource::eSubtype_isolation_source      ),
    TSubSrcKey ( "lab_host",             CSubSource::eSubtype_lab_host              ),
    TSubSrcKey ( "map",                  CSubSource::eSubtype_map                   ),
    TSubSrcKey ( "plasmid",              CSubSource::eSubtype_plasmid_name          ),
    TSubSrcKey ( "plastid",              CSubSource::eSubtype_plastid_name          ),
    TSubSrcKey ( "pop_variant",          CSubSource::eSubtype_pop_variant           ),
    TSubSrcKey ( "rearranged",           CSubSource::eSubtype_rearranged            ),
    TSubSrcKey ( "segment",              CSubSource::eSubtype_segment               ),
    TSubSrcKey ( "sex",                  CSubSource::eSubtype_sex                   ),
    TSubSrcKey ( "subclone",             CSubSource::eSubtype_subclone              ),
    TSubSrcKey ( "tissue_lib ",          CSubSource::eSubtype_tissue_lib            ),
    TSubSrcKey ( "tissue_type",          CSubSource::eSubtype_tissue_type           ),
    TSubSrcKey ( "transgenic",           CSubSource::eSubtype_transgenic            ),
    TSubSrcKey ( "transposon",           CSubSource::eSubtype_transposon_name       )
};

typedef CStaticArrayMap <const char*, const CSubSource::ESubtype, PNocase> TSubSrcMap;
static const TSubSrcMap sm_SubSrcKeys (subsrc_key_to_subtype, sizeof (subsrc_key_to_subtype));


typedef pair <const char *, const COrgMod::ESubtype> TOrgModKey;

static const TOrgModKey orgmod_key_to_subtype [] = {
    TOrgModKey ( "acronym",          COrgMod::eSubtype_acronym          ),
    TOrgModKey ( "anamorph",         COrgMod::eSubtype_anamorph         ),
    TOrgModKey ( "authority",        COrgMod::eSubtype_authority        ),
    TOrgModKey ( "biotype",          COrgMod::eSubtype_biotype          ),
    TOrgModKey ( "biovar",           COrgMod::eSubtype_biovar           ),
    TOrgModKey ( "breed",            COrgMod::eSubtype_breed            ),
    TOrgModKey ( "chemovar",         COrgMod::eSubtype_chemovar         ),
    TOrgModKey ( "common",           COrgMod::eSubtype_common           ),
    TOrgModKey ( "cultivar",         COrgMod::eSubtype_cultivar         ),
    TOrgModKey ( "dosage",           COrgMod::eSubtype_dosage           ),
    TOrgModKey ( "ecotype",          COrgMod::eSubtype_ecotype          ),
    TOrgModKey ( "forma",            COrgMod::eSubtype_forma            ),
    TOrgModKey ( "forma_specialis",  COrgMod::eSubtype_forma_specialis  ),
    TOrgModKey ( "gb_acronym",       COrgMod::eSubtype_gb_acronym       ),
    TOrgModKey ( "gb_anamorph",      COrgMod::eSubtype_gb_anamorph      ),
    TOrgModKey ( "gb_synonym",       COrgMod::eSubtype_gb_synonym       ),
    TOrgModKey ( "group",            COrgMod::eSubtype_group            ),
    TOrgModKey ( "isolate",          COrgMod::eSubtype_isolate          ),
    TOrgModKey ( "nat_host",         COrgMod::eSubtype_nat_host         ),
    TOrgModKey ( "old_lineage",      COrgMod::eSubtype_old_lineage      ),
    TOrgModKey ( "old_name",         COrgMod::eSubtype_old_name         ),
    TOrgModKey ( "pathovar",         COrgMod::eSubtype_pathovar         ),
    TOrgModKey ( "serogroup",        COrgMod::eSubtype_serogroup        ),
    TOrgModKey ( "serotype",         COrgMod::eSubtype_serotype         ),
    TOrgModKey ( "serovar",          COrgMod::eSubtype_serovar          ),
    TOrgModKey ( "specimen_voucher", COrgMod::eSubtype_specimen_voucher ),
    TOrgModKey ( "strain",           COrgMod::eSubtype_strain           ),
    TOrgModKey ( "sub_species",      COrgMod::eSubtype_sub_species      ),
    TOrgModKey ( "subgroup",         COrgMod::eSubtype_subgroup         ),
    TOrgModKey ( "substrain",        COrgMod::eSubtype_substrain        ),
    TOrgModKey ( "subtype",          COrgMod::eSubtype_subtype          ),
    TOrgModKey ( "synonym",          COrgMod::eSubtype_synonym          ),
    TOrgModKey ( "teleomorph",       COrgMod::eSubtype_teleomorph       ),
    TOrgModKey ( "type",             COrgMod::eSubtype_type             ),
    TOrgModKey ( "variety",          COrgMod::eSubtype_variety          )
};

typedef CStaticArrayMap <const char*, const COrgMod::ESubtype, PNocase> TOrgModMap;
static const TOrgModMap sm_OrgModKeys (orgmod_key_to_subtype, sizeof (orgmod_key_to_subtype));


typedef pair <const char *, const CSeqFeatData::EBond> TBondKey;

static const TBondKey bond_key_to_subtype [] = {
    TBondKey ( "disulfide",         CSeqFeatData::eBond_disulfide  ),
    TBondKey ( "other",             CSeqFeatData::eBond_other      ),
    TBondKey ( "thioether",         CSeqFeatData::eBond_thioether  ),
    TBondKey ( "thiolester",        CSeqFeatData::eBond_thiolester ),
    TBondKey ( "xlink",             CSeqFeatData::eBond_xlink      )
};

typedef CStaticArrayMap <const char*, const CSeqFeatData::EBond, PNocase> TBondMap;
static const TBondMap sm_BondKeys (bond_key_to_subtype, sizeof (bond_key_to_subtype));


typedef pair <const char *, const CSeqFeatData::ESite> TSiteKey;

static const TSiteKey site_key_to_subtype [] = {
    TSiteKey ( "acetylation",                 CSeqFeatData::eSite_acetylation                 ),
    TSiteKey ( "active",                      CSeqFeatData::eSite_active                      ),
    TSiteKey ( "amidation",                   CSeqFeatData::eSite_amidation                   ),
    TSiteKey ( "binding",                     CSeqFeatData::eSite_binding                     ),
    TSiteKey ( "blocked",                     CSeqFeatData::eSite_blocked                     ),
    TSiteKey ( "cleavage",                    CSeqFeatData::eSite_cleavage                    ),
    TSiteKey ( "DNA binding",                 CSeqFeatData::eSite_dna_binding                 ),
    TSiteKey ( "gamma carboxyglutamic acid",  CSeqFeatData::eSite_gamma_carboxyglutamic_acid  ),
    TSiteKey ( "glycosylation",               CSeqFeatData::eSite_glycosylation               ),
    TSiteKey ( "hydroxylation",               CSeqFeatData::eSite_hydroxylation               ),
    TSiteKey ( "inhibit",                     CSeqFeatData::eSite_inhibit                     ),
    TSiteKey ( "lipid binding",               CSeqFeatData::eSite_lipid_binding               ),
    TSiteKey ( "metal binding",               CSeqFeatData::eSite_metal_binding               ),
    TSiteKey ( "methylation",                 CSeqFeatData::eSite_methylation                 ),
    TSiteKey ( "modified",                    CSeqFeatData::eSite_modified                    ),
    TSiteKey ( "mutagenized",                 CSeqFeatData::eSite_mutagenized                 ),
    TSiteKey ( "myristoylation",              CSeqFeatData::eSite_myristoylation              ),
    TSiteKey ( "np binding",                  CSeqFeatData::eSite_np_binding                  ),
    TSiteKey ( "other",                       CSeqFeatData::eSite_other                       ),
    TSiteKey ( "oxidative deamination",       CSeqFeatData::eSite_oxidative_deamination       ),
    TSiteKey ( "phosphorylation",             CSeqFeatData::eSite_phosphorylation             ),
    TSiteKey ( "pyrrolidone carboxylic acid", CSeqFeatData::eSite_pyrrolidone_carboxylic_acid ),
    TSiteKey ( "signal peptide",              CSeqFeatData::eSite_signal_peptide              ),
    TSiteKey ( "sulfatation",                 CSeqFeatData::eSite_sulfatation                 ),
    TSiteKey ( "transit peptide",             CSeqFeatData::eSite_transit_peptide             ),
    TSiteKey ( "transmembrane region",        CSeqFeatData::eSite_transmembrane_region        )
};

typedef CStaticArrayMap <const char*, const CSeqFeatData::ESite, PNocase> TSiteMap;
static const TSiteMap sm_SiteKeys (site_key_to_subtype, sizeof (site_key_to_subtype));


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
    TTrnaKey ( "fMet",           'M' ),
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
    TTrnaKey ( "Sec",            'U' ),
    TTrnaKey ( "Selenocysteine", 'U' ),
    TTrnaKey ( "Ser",            'S' ),
    TTrnaKey ( "Serine",         'S' ),
    TTrnaKey ( "Ter",            '*' ),
    TTrnaKey ( "TERM",           '*' ),
    TTrnaKey ( "Termination",    '*' ),
    TTrnaKey ( "Thr",            'T' ),
    TTrnaKey ( "Threonine",      'T' ),
    TTrnaKey ( "Trp",            'W' ),
    TTrnaKey ( "Tryptophan",     'W' ),
    TTrnaKey ( "Tyr",            'Y' ),
    TTrnaKey ( "Tyrosine",       'Y' ),
    TTrnaKey ( "Val",            'V' ),
    TTrnaKey ( "Valine",         'V' ),
    TTrnaKey ( "Xxx",            'X' )
};

typedef CStaticArrayMap <const char*, const int, PNocase> TTrnaMap;
static const TTrnaMap sm_TrnaKeys (trna_key_to_subtype, sizeof (trna_key_to_subtype));


static const char * single_key_list [] = {
    "environmental_sample",
    "germline",
    "pseudo",
    "rearranged",
    "transgenic"
};

typedef CStaticArraySet <const char*, PNocase> TSingleSet;
static const TSingleSet sc_SingleKeys (single_key_list, sizeof (single_key_list));


// constructor
CFeature_table_reader_imp::CFeature_table_reader_imp(void)
{
}

// destructor
CFeature_table_reader_imp::~CFeature_table_reader_imp(void)
{
}


bool CFeature_table_reader_imp::x_ParseFeatureTableLine (const string& line, Int4* startP, Int4* stopP,
                                                         bool* partial5P, bool* partial3P, bool* ispointP,
                                                         string& featP, string& qualP, string& valP, Int4 offset)

{
    SIZE_TYPE      numtkns;
    bool           badNumber = false;
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

    tkns.clear ();
    NStr::Tokenize (line, "\t", tkns);
    numtkns = tkns.size ();

    if (numtkns > 0) {
        start = tkns [0];
    }
    if (numtkns > 1) {
        stop = tkns [1];
    }
    if (numtkns > 2) {
        feat = tkns [2];
    }
    if (numtkns > 3) {
        qual = tkns [3];
    }
    if (numtkns > 4) {
        val = tkns [4];
    }
    if (numtkns > 5) {
        stnd = tkns [5];
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
            startv = NStr::StringToLong (start);
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
            stopv = NStr::StringToLong (stop);
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
            }
        }
    }

    *startP = startv + offset;
    *stopP = stopv + offset;
    *partial5P = partial5;
    *partial3P = partial3;
    *ispointP = ispoint;
    featP = feat;
    qualP = qual;
    valP = val;

    return true;
}


bool CFeature_table_reader_imp::x_AddQualifierToGene (CSeqFeatData& sfdata,
                                                      EQual qtype, const string& val)

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
        default:
            break;
    }
    return false;
}


bool CFeature_table_reader_imp::x_AddQualifierToCdregion (CRef<CSeq_feat> sfp, CSeqFeatData& sfdata,
                                                          EQual qtype, const string& val)

{
    CCdregion& crp = sfdata.SetCdregion ();
    switch (qtype) {
        case eQual_codon_start:
            {
                int frame = NStr::StringToInt (val);
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
            return true;
        case eQual_transl_except:
            return true;
        default:
            break;
    }
    return false;
}


bool CFeature_table_reader_imp::x_StringIsJustQuotes (const string& str)

{
    ITERATE (string, it, str) {
      char ch = *it;
      if (ch > ' ' && ch != '"' && ch != '\'') return false;
    }

    return true;
}


int CFeature_table_reader_imp::x_ParseTrnaString (const string& val)

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


bool CFeature_table_reader_imp::x_AddQualifierToRna (CSeqFeatData& sfdata,
                                                     EQual qtype, const string& val)

{
    CRNA_ref& rrp = sfdata.SetRna ();
    CRNA_ref::EType rnatyp = rrp.GetType ();
    switch (rnatyp) {
        case CRNA_ref::eType_premsg:
        case CRNA_ref::eType_mRNA:
        case CRNA_ref::eType_rRNA:
        case CRNA_ref::eType_snRNA:
        case CRNA_ref::eType_scRNA:
        case CRNA_ref::eType_snoRNA:
        case CRNA_ref::eType_other:
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
                    break;
                case eQual_codon_recognized:
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


bool CFeature_table_reader_imp::x_AddQualifierToImp (CRef<CSeq_feat> sfp, CSeqFeatData& sfdata,
                                                     EQual qtype, const string& qual, const string& val)

{
    switch (qtype) {
        case eQual_allele:
        case eQual_bound_moiety:
        case eQual_clone:
        case eQual_compare:
        case eQual_cons_splice:
        case eQual_direction:
        case eQual_EC_number:
        case eQual_frequency:
        case eQual_function:
        case eQual_insertion_seq:
        case eQual_label:
        case eQual_map:
        case eQual_number:
        case eQual_old_locus_tag:
        case eQual_operon:
        case eQual_organism:
        case eQual_PCR_conditions:
        case eQual_phenotype:
        case eQual_product:
        case eQual_replace:
        case eQual_rpt_family:
        case eQual_rpt_type:
        case eQual_rpt_unit:
        case eQual_standard_name:
        case eQual_transposon:
        case eQual_usedin:
            {
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
        default:
            break;
    }

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
                        {
                            CSeq_feat::TExt& ext = sfp->SetExt ();
                            CObject_id& obj = ext.SetType ();
                            if ((! obj.IsStr ()) || obj.GetStr ().empty ()) {
                                obj.SetStr ("dbSnpSynonymyData");
                            }
                            ext.AddField (qual, val, CUser_object::eParse_Number);
                            return true;
                        }
                    default:
                        break;
                }
            }
        case CSeqFeatData::eSubtype_STS:
            {
                switch (qtype) {
                    case eQual_sts_aliases:
                    case eQual_sts_dsegs:
                    case eQual_weight:
                        {
                            CSeq_feat::TExt& ext = sfp->SetExt ();
                            CObject_id& obj = ext.SetType ();
                            if ((! obj.IsStr ()) || obj.GetStr ().empty ()) {
                                obj.SetStr ("stsUserObject");
                            }
                            ext.AddField (qual, val, CUser_object::eParse_Number);
                            return true;
                        }
                    default:
                        break;
                }
            }
        case CSeqFeatData::eSubtype_misc_feature:
            {
                switch (qtype) {
                    case eQual_bac_ends:
                    case eQual_clone_id:
                    case eQual_method:
                    case eQual_sequence:
                    case eQual_STS:
                    case eQual_weight:
                        {
                            CSeq_feat::TExt& ext = sfp->SetExt ();
                            CObject_id& obj = ext.SetType ();
                            if ((! obj.IsStr ()) || obj.GetStr ().empty ()) {
                                obj.SetStr ("cloneUserObject");
                            }
                            ext.AddField (qual, val, CUser_object::eParse_Number);
                            return true;
                        }
                    default:
                        break;
                }
            }
        default:
            break;
    }

    return false;
}


bool CFeature_table_reader_imp::x_AddQualifierToBioSrc (CSeqFeatData& sfdata,
                                                        EOrgRef rtype, const string& val)

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
                int code = NStr::StringToInt (val);
                onp.SetGcode (code);
                return true;
            }
        case eOrgRef_mgcode:
            {
                CBioSource::TOrg& orp = bsp.SetOrg ();
                COrg_ref::TOrgname& onp = orp.SetOrgname ();
                int code = NStr::StringToInt (val);
                onp.SetMgcode (code);
                return true;
            }
        default:
            break;
    }
    return false;
}


bool CFeature_table_reader_imp::x_AddQualifierToBioSrc (CSeqFeatData& sfdata,
                                                        CSubSource::ESubtype stype, const string& val)

{
    CBioSource& bsp = sfdata.SetBiosrc ();
    CBioSource::TSubtype& slist = bsp.SetSubtype ();
    CRef<CSubSource> ssp (new CSubSource);
    ssp->SetSubtype (stype);
    ssp->SetName (val);
    slist.push_back (ssp);
    return true;
}


bool CFeature_table_reader_imp::x_AddQualifierToBioSrc (CSeqFeatData& sfdata,
                                                        COrgMod::ESubtype  mtype, const string& val)

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


bool CFeature_table_reader_imp::x_AddQualifierToFeature (CRef<CSeq_feat> sfp,
                                                         const string& qual, const string& val)

{
    CSeqFeatData&          sfdata = sfp->SetData ();
    CSeqFeatData::E_Choice typ = sfdata.Which ();

    if (typ == CSeqFeatData::e_Biosrc) {

        TOrgRefMap::const_iterator o_iter = sm_OrgRefKeys.find (qual.c_str ());
        if (o_iter != sm_OrgRefKeys.end ()) {
            EOrgRef rtype = o_iter->second;
            if (x_AddQualifierToBioSrc (sfdata, rtype, val)) return true;

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
                    if (x_AddQualifierToCdregion (sfp, sfdata, qtype, val)) return true;
                    break;
                case CSeqFeatData::e_Rna:
                    if (x_AddQualifierToRna (sfdata, qtype, val)) return true;
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
                        TBondMap::const_iterator b_iter = sm_BondKeys.find (val.c_str ());
                        if (b_iter != sm_BondKeys.end ()) {
                            CSeqFeatData::EBond btyp = b_iter->second;
                            sfdata.SetBond (btyp);
                            return true;
                        }
                    }
                    break;
                case CSeqFeatData::e_Site:
                    if (qtype == eQual_site_type) {
                        TSiteMap::const_iterator s_iter = sm_SiteKeys.find (val.c_str ());
                        if (s_iter != sm_SiteKeys.end ()) {
                            CSeqFeatData::ESite styp = s_iter->second;
                            sfdata.SetSite (styp);
                            return true;
                        }
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
                case eQual_allele:
                case eQual_bound_moiety:
                case eQual_clone:
                case eQual_compare:
                case eQual_cons_splice:
                case eQual_direction:
                case eQual_EC_number:
                case eQual_frequency:
                case eQual_function:
                case eQual_insertion_seq:
                case eQual_label:
                case eQual_map:
                case eQual_number:
                case eQual_old_locus_tag:
                case eQual_operon:
                case eQual_organism:
                case eQual_PCR_conditions:
                case eQual_phenotype:
                case eQual_product:
                case eQual_protein_id:
                case eQual_replace:
                case eQual_rpt_family:
                case eQual_rpt_type:
                case eQual_rpt_unit:
                case eQual_standard_name:
                case eQual_transcript_id:
                case eQual_transposon:
                case eQual_usedin:
                    {
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
                            num = NStr::StringToNumeric (tag);
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
                         return true;
                         */
                    }
                default:
                    break;
            }
        }
    }
    return false;
}


bool CFeature_table_reader_imp::x_AddIntervalToFeature (CRef<CSeq_feat> sfp, CSeq_loc_mix *mix,
                                                        const string& seqid, Int4 start, Int4 stop,
                                                        bool partial5, bool partial3, bool ispoint)

{
    CSeq_interval::TStrand strand = eNa_strand_plus;

    if (start > stop) {
        Int4 flip = start;
        start = stop;
        stop = flip;
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
        mix->Set().push_back(loc);
    } else if (start == stop) {
        // just a point
        CRef<CSeq_loc> loc(new CSeq_loc);
        CSeq_point& point = loc->SetPnt ();
        point.SetPoint (start);
        point.SetStrand (strand);
        CSeq_id seq_id (seqid);
        point.SetId().Assign (seq_id);
        mix->Set().push_back (loc);
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
        mix->Set().push_back (loc);
    }

    if (partial5 || partial3) {
        sfp->SetPartial (true);
    }

    return true;
}


CRef<CSeq_annot> CFeature_table_reader_imp::ReadSequinFeatureTable (
    CNcbiIstream& ifs,
    const string& seqid,
    const string& annotname,
    const CFeature_table_reader::TFlags flags
)

{
    string line;
    string feat, qual, val;
    Int4 start, stop;
    bool partial5, partial3, ispoint;
    Int4 offset = 0;
    CSeqFeatData::ESubtype sbtyp = CSeqFeatData::eSubtype_bad;
    CSeqFeatData::E_Choice typ = CSeqFeatData::e_not_set;
    CRef<CSeq_annot> sap(new CSeq_annot);
    CSeq_annot::C_Data::TFtable& ftable = sap->SetData().SetFtable();
    CRef<CSeq_feat> sfp;
    CSeq_loc_mix *mix = 0;
    CT_POS_TYPE pos(0);

    if (! annotname.empty ()) {
      CAnnot_descr& descr = sap->SetDesc ();
      CRef<CAnnotdesc> annot(new CAnnotdesc);
      annot->SetName (annotname);
      descr.Set().push_back (annot);
    }

    while (ifs.good ()) {

        pos = ifs.tellg ();
        NcbiGetlineEOL (ifs, line);

        if (! line.empty ()) {
            if (line [0] == '>') {

                // if next feature table, reposition and return current sap

                ifs.seekg (pos);
                return sap;

            } else if (line [0] == '[') {

                // set offset !!!!!!!!

            } else if (x_ParseFeatureTableLine (line, &start, &stop, &partial5, &partial3,
                                                &ispoint, feat, qual, val, offset)) {

                // process line in feature table

                if ((! feat.empty ()) && start >= 0 && stop >= 0) {

                    // process start - stop - feature line

                    TFeatMap::const_iterator f_iter = sm_FeatKeys.find (feat.c_str ());
                    if (f_iter != sm_FeatKeys.end ()) {
                        sbtyp = f_iter->second;
                        if (sbtyp != CSeqFeatData::eSubtype_bad) {

                            // populate *sfp here...

                            sfp.Reset (new CSeq_feat);
                            sfp->ResetLocation ();

                            typ = CSeqFeatData::GetTypeFromSubtype (sbtyp);
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
                                        rnatyp = CRNA_ref::eType_snRNA;
                                        break;
                                    case CSeqFeatData::eSubtype_scRNA :
                                        rnatyp = CRNA_ref::eType_scRNA;
                                        break;
                                    case CSeqFeatData::eSubtype_snoRNA :
                                        rnatyp = CRNA_ref::eType_snoRNA;
                                        break;
                                    case CSeqFeatData::eSubtype_otherRNA :
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
                            }

                            ftable.push_back (sfp);

                            // now create location

                            CRef<CSeq_loc> location (new CSeq_loc);
                            mix = &(location->SetMix ());
                            sfp->SetLocation (*location);

                            // and add first interval

                            x_AddIntervalToFeature (sfp, mix, seqid, start, stop, partial5, partial3, ispoint);

                        }
                    } else {

                        // unrecognized feature key

                        if ((flags & CFeature_table_reader::fReportBadKey) != 0) {
                            ERR_POST (Warning << "Unrecognized feature " << feat);
                        }

                        if ((flags & CFeature_table_reader::fKeepBadKey) != 0) {
                            sfp.Reset (new CSeq_feat);
                            sfp->ResetLocation ();
                            sfp->SetData ().Select (CSeqFeatData::e_Imp);
                            CSeqFeatData& sfdata = sfp->SetData ();
                            CImp_feat_Base& imp = sfdata.SetImp ();
                            imp.SetKey (feat);
                            ftable.push_back (sfp);
                            CRef<CSeq_loc> location (new CSeq_loc);
                            mix = &(location->SetMix ());
                            sfp->SetLocation (*location);
                            x_AddIntervalToFeature (sfp, mix, seqid, start, stop, partial5, partial3, ispoint);
                        }
                    }

                } else if (start >= 0 && stop >= 0 && feat.empty () && qual.empty () && val.empty ()) {

                    // process start - stop multiple interval line

                    x_AddIntervalToFeature (sfp, mix, seqid, start, stop, partial5, partial3, ispoint);

                } else if ((! qual.empty ()) && (! val.empty ())) {

                    // process qual - val qualifier line

                    if (! x_AddQualifierToFeature (sfp, qual, val)) {

                        // unrecognized qualifier key

                        if ((flags & CFeature_table_reader::fReportBadKey) != 0) {
                            ERR_POST (Warning << "Unrecognized qualifier " << qual);
                        }

                        if ((flags & CFeature_table_reader::fKeepBadKey) != 0) {
                            CSeq_feat::TQual& qlist = sfp->SetQual ();
                            CRef<CGb_qual> gbq (new CGb_qual);
                            gbq->SetQual (qual);
                            if (x_StringIsJustQuotes (val)) {
                                gbq->SetVal (kEmptyStr);
                            } else {
                                gbq->SetVal (val);
                            }
                            qlist.push_back (gbq);
                        }
                    }

                } else if ((! qual.empty ()) && (val.empty ())) {

                    // check for the few qualifiers that do not need a value

                    TSingleSet::const_iterator s_iter = sc_SingleKeys.find (qual.c_str ());
                    if (s_iter != sc_SingleKeys.end ()) {

                        x_AddQualifierToFeature (sfp, qual, val);

                    }
                } else if (! feat.empty ()) {
                
                    // unrecognized location

                    if ((flags & CFeature_table_reader::fReportBadKey) != 0) {
                        ERR_POST (Warning << "Bad location on feature " << feat <<
                                 " (start " << start << ", stop " << stop << ")");
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
    const CFeature_table_reader::TFlags flags
)

{
    CRef<CSeq_feat> sfp (new CSeq_feat);

    if (! feat.empty ()) {
        TFeatMap::const_iterator f_iter = sm_FeatKeys.find (feat.c_str ());
        if (f_iter != sm_FeatKeys.end ()) {
            CSeqFeatData::ESubtype sbtyp = f_iter->second;
            CSeqFeatData::E_Choice typ = CSeqFeatData::GetTypeFromSubtype (sbtyp);
            sfp->SetData ().Select (typ);
            CSeqFeatData& sfdata = sfp->SetData ();
            sfp->SetLocation (location);
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
                        rnatyp = CRNA_ref::eType_snRNA;
                        break;
                    case CSeqFeatData::eSubtype_scRNA :
                        rnatyp = CRNA_ref::eType_scRNA;
                        break;
                    case CSeqFeatData::eSubtype_snoRNA :
                        rnatyp = CRNA_ref::eType_snoRNA;
                        break;
                    case CSeqFeatData::eSubtype_otherRNA :
                        rnatyp = CRNA_ref::eType_other;
                        break;
                    default :
                        break;
                }
                rrp.SetType (rnatyp);

            } else if (typ == CSeqFeatData::e_Imp) {
                CImp_feat_Base& imp = sfdata.SetImp ();
                imp.SetKey (feat);
            }
            sfp->SetLocation (location);
 
        } else {

            // unrecognized feature key

            if ((flags & CFeature_table_reader::fReportBadKey) != 0) {
                ERR_POST (Warning << "Unrecognized feature " << feat);
            }

            if ((flags & CFeature_table_reader::fKeepBadKey) != 0) {
                sfp.Reset (new CSeq_feat);
                sfp->ResetLocation ();
                sfp->SetData ().Select (CSeqFeatData::e_Imp);
                CSeqFeatData& sfdata = sfp->SetData ();
                CImp_feat_Base& imp = sfdata.SetImp ();
                imp.SetKey (feat);
                sfp->SetLocation (location);
            }
        }
    }

    return sfp;
}


void CFeature_table_reader_imp::AddFeatQual (
    CRef<CSeq_feat> sfp,
    const string& qual,
    const string& val,
    const CFeature_table_reader::TFlags flags
)

{
    if ((! qual.empty ()) && (! val.empty ())) {

        if (! x_AddQualifierToFeature (sfp, qual, val)) {

            // unrecognized qualifier key

            if ((flags & CFeature_table_reader::fReportBadKey) != 0) {
                ERR_POST (Warning << "Unrecognized qualifier " << qual);
            }

            if ((flags & CFeature_table_reader::fKeepBadKey) != 0) {
                CSeq_feat::TQual& qlist = sfp->SetQual ();
                CRef<CGb_qual> gbq (new CGb_qual);
                gbq->SetQual (qual);
                if (x_StringIsJustQuotes (val)) {
                    gbq->SetVal (kEmptyStr);
                } else {
                    gbq->SetVal (val);
                }
                qlist.push_back (gbq);
            }
        }

    } else if ((! qual.empty ()) && (val.empty ())) {

        // check for the few qualifiers that do not need a value

        TSingleSet::const_iterator s_iter = sc_SingleKeys.find (qual.c_str ());
        if (s_iter != sc_SingleKeys.end ()) {

            x_AddQualifierToFeature (sfp, qual, val);

        }
    }
}


// public access functions

CRef<CSeq_annot> CFeature_table_reader::ReadSequinFeatureTable (
    CNcbiIstream& ifs,
    const string& seqid,
    const string& annotname,
    const TFlags flags
)

{
    // just read features from 5-column table

    CRef<CSeq_annot> sap = x_GetImplementation ().ReadSequinFeatureTable (ifs, seqid, annotname, flags);

    // go through all features and demote single interval seqlocmix to seqlocint

    for (CTypeIterator<CSeq_feat> fi(*sap); fi; ++fi) {
        CSeq_feat& feat = *fi;
        CSeq_loc& location = feat.SetLocation ();
        if (location.IsMix ()) {
            CSeq_loc_mix& mx = location.SetMix ();
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
    const TFlags flags
)

{
    string line, fst, scd, seqid, annotname;
    CT_POS_TYPE pos(0);

    // first look for >Feature line, extract seqid and optional annotname

    while (seqid.empty () && ifs.good ()) {

        pos = ifs.tellg ();
        NcbiGetlineEOL (ifs, line);

        if (! line.empty ()) {
            if (line [0] == '>') {
                if (NStr::StartsWith (line, ">Feature")) {
                    NStr::SplitInTwo (line, " ", fst, scd);
                    NStr::SplitInTwo (scd, " ", seqid, annotname);
                }
            }
        }
    }

    // then read features from 5-column table

    return ReadSequinFeatureTable (ifs, seqid, annotname, flags);

}

CRef<CSeq_feat> CFeature_table_reader::CreateSeqFeat (
    const string& feat,
    CSeq_loc& location,
    const TFlags flags
)

{
    return x_GetImplementation ().CreateSeqFeat (feat, location, flags);
}


void CFeature_table_reader::AddFeatQual (
    CRef<CSeq_feat> sfp,
    const string& qual,
    const string& val,
    const CFeature_table_reader::TFlags flags
)

{
    x_GetImplementation ().AddFeatQual (sfp, qual, val, flags);
}


END_objects_SCOPE
END_NCBI_SCOPE
