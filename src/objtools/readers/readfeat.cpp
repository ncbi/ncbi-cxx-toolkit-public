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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>

#include <serial/iterator.hpp>
#include <serial/objistrasn.hpp>

// Objects includes
#include <objects/general/Int_fuzz.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/RNA_ref.hpp>

#include <objtools/readers/readfeat.hpp>


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class /* NCBI_XOBJREAD_EXPORT */ CFeature_table_reader_imp
{
public:
    enum EQual {
        eQual_bad,
        eQual_allele,
        eQual_anticodon,
        eQual_bond_type,
        eQual_bound_moiety,
        eQual_citation,
        eQual_clone,
        eQual_codon_start,
        eQual_cons_splice,
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
        eQual_locus_tag,
        eQual_macronuclear,
        eQual_map,
        eQual_MEDLINE,
        eQual_mod_base,
        eQual_muid,
        eQual_note,
        eQual_number,
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
        eQual_site_type,
        eQual_standard_name,
        eQual_transcript_id,
        eQual_transl_except,
        eQual_transl_table,
        eQual_translation,
        eQual_transposon,
        eQual_usedin
    };

    enum ESubSrc {
        eSubSrc_bad,
        eSubSrc_cell_line,
        eSubSrc_cell_type,
        eSubSrc_chromosome,
        eSubSrc_clone,
        eSubSrc_clone_lib,
        eSubSrc_country,
        eSubSrc_dev_stage,
        eSubSrc_endogenous_virus_name,
        eSubSrc_environmental_sample,
        eSubSrc_frequency,
        eSubSrc_genotype,
        eSubSrc_germline,
        eSubSrc_haplotype,
        eSubSrc_insertion_seq_name,
        eSubSrc_isolation_source,
        eSubSrc_lab_host,
        eSubSrc_map,
        eSubSrc_plasmid_name,
        eSubSrc_plastid_name,
        eSubSrc_pop_variant,
        eSubSrc_rearranged,
        eSubSrc_segment,
        eSubSrc_sex,
        eSubSrc_subclone,
        eSubSrc_tissue_lib,
        eSubSrc_tissue_type,
        eSubSrc_transgenic,
        eSubSrc_transposon_name
    };

    enum EOrgMod {
        eOrgMod_bad,
        eOrgMod_acronym,
        eOrgMod_anamorph,
        eOrgMod_authority,
        eOrgMod_biotype,
        eOrgMod_biovar,
        eOrgMod_breed,
        eOrgMod_chemovar,
        eOrgMod_common,
        eOrgMod_cultivar,
        eOrgMod_dosage,
        eOrgMod_ecotype,
        eOrgMod_forma_specialis,
        eOrgMod_forma,
        eOrgMod_gb_acronym,
        eOrgMod_gb_anamorph,
        eOrgMod_gb_synonym,
        eOrgMod_group,
        eOrgMod_isolate,
        eOrgMod_nat_host,
        eOrgMod_pathovar,
        eOrgMod_serogroup,
        eOrgMod_serotype,
        eOrgMod_serovar,
        eOrgMod_specimen_voucher,
        eOrgMod_strain,
        eOrgMod_sub_species,
        eOrgMod_subgroup,
        eOrgMod_substrain,
        eOrgMod_subtype,
        eOrgMod_synonym,
        eOrgMod_teleomorph,
        eOrgMod_type,
        eOrgMod_variety
    };

    typedef map< string, CSeqFeatData::ESubtype > TFeatReaderMap;
    typedef map< string, EQual > TQualReaderMap;
    typedef map< string, ESubSrc > TSubSrcReaderMap;
    typedef map< string, EOrgMod > TOrgModReaderMap;

    // constructor
    CFeature_table_reader_imp(void);
    // destructor
    ~CFeature_table_reader_imp(void);

    // read 5-column feature table and return Seq-annot
    CRef<CSeq_annot> ReadSequinFeatureTable (CNcbiIfstream& ifs,
                                             const string& seqid,
                                             const string& annotname);

private:
    // Prohibit copy constructor and assignment operator
    CFeature_table_reader_imp(const CFeature_table_reader_imp& value);
    CFeature_table_reader_imp& operator=(const CFeature_table_reader_imp& value);

    bool x_ParseFeatureTableLine (const string& line, Int4* startP, Int4* stopP,
                                  bool* partial5P, bool* partial3P, string& featP,
                                  string& qualP, string& valP, Int4 offset);

    bool x_AddIntervalToFeature (CRef<CSeq_feat> sfp, CSeq_loc_mix *mix,
                                 const string& seqid, Int4 start,
                                 Int4 stop, bool partial5, bool partial3);

    bool x_AddQualifierToFeature (CRef<CSeq_feat> sfp,
                                  const string& qual, const string& val);

    bool x_AddQualifierToGene (CSeqFeatData& sfdata,
                               EQual qtype, const string& val);
    bool x_AddQualifierToCdregion (CRef<CSeq_feat> sfp, CSeqFeatData& sfdata,
                                   EQual qtype, const string& val);
    bool x_AddQualifierToRna (CSeqFeatData& sfdata,
                              EQual qtype, const string& val);

    TFeatReaderMap m_FeatKeys;
    TQualReaderMap m_QualKeys;
    TSubSrcReaderMap m_SubSrcKeys;
    TOrgModReaderMap m_OrgModKeys;
};

auto_ptr<CFeature_table_reader_imp> CFeature_table_reader::sm_Implementation;

void CFeature_table_reader::x_InitImplementation()
{
    static CFastMutex s_Implementation_mutex;
    CFastMutexGuard   LOCK(s_Implementation_mutex);
    if ( !sm_Implementation.get() ) {
        sm_Implementation.reset(new CFeature_table_reader_imp());
    }
}


typedef struct featinit {
    const char *           key;
    CSeqFeatData::ESubtype subtype;
} FeatInit;

static FeatInit feat_key_to_subtype [] = {
    { "-10_signal",         CSeqFeatData::eSubtype_10_signal          },
    { "-35_signal",         CSeqFeatData::eSubtype_35_signal          },
    { "3'clip",             CSeqFeatData::eSubtype_3clip              },
    { "3'UTR",              CSeqFeatData::eSubtype_3UTR               },
    { "5'clip",             CSeqFeatData::eSubtype_5clip              },
    { "5'UTR",              CSeqFeatData::eSubtype_5UTR               },
    { "attenuator",         CSeqFeatData::eSubtype_attenuator         },
    { "Bond",               CSeqFeatData::eSubtype_bond               },
    { "CAAT_signal",        CSeqFeatData::eSubtype_CAAT_signal        },
    { "CDS",                CSeqFeatData::eSubtype_cdregion           },
    { "Cit",                CSeqFeatData::eSubtype_pub                },
    { "Comment",            CSeqFeatData::eSubtype_comment            },
    { "conflict",           CSeqFeatData::eSubtype_conflict           },
    { "C_region",           CSeqFeatData::eSubtype_C_region           },
    { "D-loop",             CSeqFeatData::eSubtype_D_loop             },
    { "D_segment",          CSeqFeatData::eSubtype_D_segment          },
    { "enhancer",           CSeqFeatData::eSubtype_enhancer           },
    { "exon",               CSeqFeatData::eSubtype_exon               },
    { "GC_signal",          CSeqFeatData::eSubtype_GC_signal          },
    { "gene",               CSeqFeatData::eSubtype_gene               },
    { "Het",                CSeqFeatData::eSubtype_het                },
    { "iDNA",               CSeqFeatData::eSubtype_iDNA               },
    { "intron",             CSeqFeatData::eSubtype_intron             },
    { "J_segment",          CSeqFeatData::eSubtype_J_segment          },
    { "LTR",                CSeqFeatData::eSubtype_LTR                },
    { "mat_peptide",        CSeqFeatData::eSubtype_mat_peptide_aa     },
    { "mat_peptide_nt",     CSeqFeatData::eSubtype_mat_peptide        },
    { "misc_binding",       CSeqFeatData::eSubtype_misc_binding       },
    { "misc_difference",    CSeqFeatData::eSubtype_misc_difference    },
    { "misc_feature",       CSeqFeatData::eSubtype_misc_feature       },
    { "misc_recomb",        CSeqFeatData::eSubtype_misc_recomb        },
    { "misc_RNA",           CSeqFeatData::eSubtype_otherRNA           },
    { "misc_signal",        CSeqFeatData::eSubtype_misc_signal        },
    { "misc_structure",     CSeqFeatData::eSubtype_misc_structure     },
    { "modified_base",      CSeqFeatData::eSubtype_modified_base      },
    { "mRNA",               CSeqFeatData::eSubtype_mRNA               },
    { "NonStdRes",          CSeqFeatData::eSubtype_non_std_residue    },
    { "Num",                CSeqFeatData::eSubtype_num                },
    { "N_region",           CSeqFeatData::eSubtype_N_region           },
    { "old_sequence",       CSeqFeatData::eSubtype_old_sequence       },
    { "polyA_signal",       CSeqFeatData::eSubtype_polyA_signal       },
    { "polyA_site",         CSeqFeatData::eSubtype_polyA_site         },
    { "precursor_RNA",      CSeqFeatData::eSubtype_preRNA             },
    { "pre_RNA",            CSeqFeatData::eSubtype_preRNA             },
    { "preprotein",         CSeqFeatData::eSubtype_preprotein         },
    { "primer_bind",        CSeqFeatData::eSubtype_primer_bind        },
    { "prim_transcript",    CSeqFeatData::eSubtype_prim_transcript    },
    { "promoter",           CSeqFeatData::eSubtype_promoter           },
    { "Protein",            CSeqFeatData::eSubtype_prot               },
    { "protein_bind",       CSeqFeatData::eSubtype_protein_bind       },
    { "RBS",                CSeqFeatData::eSubtype_RBS                },
    { "REFERENCE",          CSeqFeatData::eSubtype_pub                },
    { "Region",             CSeqFeatData::eSubtype_region             },
    { "repeat_region",      CSeqFeatData::eSubtype_repeat_region      },
    { "repeat_unit",        CSeqFeatData::eSubtype_repeat_unit        },
    { "rep_origin",         CSeqFeatData::eSubtype_rep_origin         },
    { "rRNA",               CSeqFeatData::eSubtype_rRNA               },
    { "Rsite",              CSeqFeatData::eSubtype_rsite              },
    { "satellite",          CSeqFeatData::eSubtype_satellite          },
    { "scRNA",              CSeqFeatData::eSubtype_scRNA              },
    { "SecStr",             CSeqFeatData::eSubtype_psec_str           },
    { "sig_peptide",        CSeqFeatData::eSubtype_sig_peptide_aa     },
    { "sig_peptide_nt",     CSeqFeatData::eSubtype_sig_peptide        },
    { "Site",               CSeqFeatData::eSubtype_site               },
    { "Site-ref",           CSeqFeatData::eSubtype_site_ref           },
    { "snoRNA",             CSeqFeatData::eSubtype_snoRNA             },
    { "snRNA",              CSeqFeatData::eSubtype_snRNA              },
    { "source",             CSeqFeatData::eSubtype_source             },
    { "Src",                CSeqFeatData::eSubtype_biosrc             },
    { "stem_loop",          CSeqFeatData::eSubtype_stem_loop          },
    { "STS",                CSeqFeatData::eSubtype_STS                },
    { "S_region",           CSeqFeatData::eSubtype_S_region           },
    { "TATA_signal",        CSeqFeatData::eSubtype_TATA_signal        },
    { "terminator",         CSeqFeatData::eSubtype_terminator         },
    { "transit_peptide",    CSeqFeatData::eSubtype_transit_peptide_aa },
    { "transit_peptide_nt", CSeqFeatData::eSubtype_transit_peptide    },
    { "tRNA",               CSeqFeatData::eSubtype_tRNA               },
    { "TxInit",             CSeqFeatData::eSubtype_txinit             },
    { "unsure",             CSeqFeatData::eSubtype_unsure             },
    { "User",               CSeqFeatData::eSubtype_user               },
    { "variation",          CSeqFeatData::eSubtype_variation          },
    { "virion",             CSeqFeatData::eSubtype_virion             },
    { "V_region",           CSeqFeatData::eSubtype_V_region           },
    { "V_segment",          CSeqFeatData::eSubtype_V_segment          },
    { "Xref",               CSeqFeatData::eSubtype_seq                },
    { "",                   CSeqFeatData::eSubtype_bad                }
};

typedef struct qualinit {
    const char *                         key;
    CFeature_table_reader_imp::EQual subtype;
} QualInit;

static QualInit qual_key_to_subtype [] = {
    { "allele",               CFeature_table_reader_imp::eQual_allele               },
    { "anticodon",            CFeature_table_reader_imp::eQual_anticodon            },
    { "bond_type",            CFeature_table_reader_imp::eQual_bond_type            },
    { "bound_moiety",         CFeature_table_reader_imp::eQual_bound_moiety         },
    { "citation",             CFeature_table_reader_imp::eQual_citation             },
    { "clone",                CFeature_table_reader_imp::eQual_clone                },
    { "codon_start",          CFeature_table_reader_imp::eQual_codon_start          },
    { "cons_splice",          CFeature_table_reader_imp::eQual_cons_splice          },
    { "db_xref",              CFeature_table_reader_imp::eQual_db_xref              },
    { "direction",            CFeature_table_reader_imp::eQual_direction            },
    { "EC_number",            CFeature_table_reader_imp::eQual_EC_number            },
    { "evidence",             CFeature_table_reader_imp::eQual_evidence             },
    { "exception",            CFeature_table_reader_imp::eQual_exception            },
    { "frequency",            CFeature_table_reader_imp::eQual_frequency            },
    { "function",             CFeature_table_reader_imp::eQual_function             },
    { "gene",                 CFeature_table_reader_imp::eQual_gene                 },
    { "gene_desc",            CFeature_table_reader_imp::eQual_gene_desc            },
    { "gene_syn",             CFeature_table_reader_imp::eQual_gene_syn             },
    { "go_component",         CFeature_table_reader_imp::eQual_go_component         },
    { "go_function",          CFeature_table_reader_imp::eQual_go_function          },
    { "go_process",           CFeature_table_reader_imp::eQual_go_process           },
    { "insertion_seq",        CFeature_table_reader_imp::eQual_insertion_seq        },
    { "label",                CFeature_table_reader_imp::eQual_label                },
    { "locus_tag",            CFeature_table_reader_imp::eQual_locus_tag            },
    { "macronuclear",         CFeature_table_reader_imp::eQual_macronuclear         },
    { "map",                  CFeature_table_reader_imp::eQual_map                  },
    { "mod_base",             CFeature_table_reader_imp::eQual_mod_base             },
    { "note",                 CFeature_table_reader_imp::eQual_note                 },
    { "number",               CFeature_table_reader_imp::eQual_number               },
    { "organism",             CFeature_table_reader_imp::eQual_organism             },
    { "partial",              CFeature_table_reader_imp::eQual_partial              },
    { "PCR_conditions",       CFeature_table_reader_imp::eQual_PCR_conditions       },
    { "phenotype",            CFeature_table_reader_imp::eQual_phenotype            },
    { "product",              CFeature_table_reader_imp::eQual_product              },
    { "protein_id",           CFeature_table_reader_imp::eQual_protein_id           },
    { "prot_desc",            CFeature_table_reader_imp::eQual_prot_desc            },
    { "pseudo",               CFeature_table_reader_imp::eQual_pseudo               },
    { "replace",              CFeature_table_reader_imp::eQual_replace              },
    { "rpt_family",           CFeature_table_reader_imp::eQual_rpt_family           },
    { "rpt_type",             CFeature_table_reader_imp::eQual_rpt_type             },
    { "rpt_unit",             CFeature_table_reader_imp::eQual_rpt_unit             },
    { "standard_name",        CFeature_table_reader_imp::eQual_standard_name        },
    { "transcript_id",        CFeature_table_reader_imp::eQual_transcript_id        },
    { "transl_except",        CFeature_table_reader_imp::eQual_transl_except        },
    { "transl_table",         CFeature_table_reader_imp::eQual_transl_table         },
    { "translation",          CFeature_table_reader_imp::eQual_translation          },
    { "transposon",           CFeature_table_reader_imp::eQual_transposon           },
    { "usedin",               CFeature_table_reader_imp::eQual_usedin               },
    { "",                     CFeature_table_reader_imp::eQual_bad                  }
};

typedef struct subsrcinit {
    const char *                       key;
    CFeature_table_reader_imp::ESubSrc subtype;
} SubSrcInit;

static SubSrcInit subsrc_key_to_subtype [] = {
    { "cell_line",            CFeature_table_reader_imp::eSubSrc_cell_line             },
    { "cell_type",            CFeature_table_reader_imp::eSubSrc_cell_type             },
    { "chromosome",           CFeature_table_reader_imp::eSubSrc_chromosome            },
    { "clone",                CFeature_table_reader_imp::eSubSrc_clone                 },
    { "clone_lib",            CFeature_table_reader_imp::eSubSrc_clone_lib             },
    { "country",              CFeature_table_reader_imp::eSubSrc_country               },
    { "dev_stage",            CFeature_table_reader_imp::eSubSrc_dev_stage             },
    { "endogenous_virus",     CFeature_table_reader_imp::eSubSrc_endogenous_virus_name },
    { "environmental_sample", CFeature_table_reader_imp::eSubSrc_environmental_sample  },
    { "frequency",            CFeature_table_reader_imp::eSubSrc_frequency             },
    { "genotype",             CFeature_table_reader_imp::eSubSrc_genotype              },
    { "germline",             CFeature_table_reader_imp::eSubSrc_germline              },
    { "haplotype",            CFeature_table_reader_imp::eSubSrc_haplotype             },
    { "insertion_seq",        CFeature_table_reader_imp::eSubSrc_insertion_seq_name    },
    { "isolation_source",     CFeature_table_reader_imp::eSubSrc_isolation_source      },
    { "lab_host",             CFeature_table_reader_imp::eSubSrc_lab_host              },
    { "map",                  CFeature_table_reader_imp::eSubSrc_map                   },
    { "plasmid",              CFeature_table_reader_imp::eSubSrc_plasmid_name          },
    { "plastid",              CFeature_table_reader_imp::eSubSrc_plastid_name          },
    { "pop_variant",          CFeature_table_reader_imp::eSubSrc_pop_variant           },
    { "rearranged",           CFeature_table_reader_imp::eSubSrc_rearranged            },
    { "segment",              CFeature_table_reader_imp::eSubSrc_segment               },
    { "sex",                  CFeature_table_reader_imp::eSubSrc_sex                   },
    { "subclone",             CFeature_table_reader_imp::eSubSrc_subclone              },
    { "tissue_lib ",          CFeature_table_reader_imp::eSubSrc_tissue_lib            },
    { "tissue_type",          CFeature_table_reader_imp::eSubSrc_tissue_type           },
    { "transgenic",           CFeature_table_reader_imp::eSubSrc_transgenic            },
    { "transposon",           CFeature_table_reader_imp::eSubSrc_transposon_name       },
    { "",                     CFeature_table_reader_imp::eSubSrc_bad                   }
};

typedef struct orgmodinit {
    const char *                       key;
    CFeature_table_reader_imp::EOrgMod subtype;
} OrgModInit;

static OrgModInit orgmod_key_to_subtype [] = {
    { "acronym",          CFeature_table_reader_imp::eOrgMod_acronym          },
    { "anamorph",         CFeature_table_reader_imp::eOrgMod_anamorph         },
    { "authority",        CFeature_table_reader_imp::eOrgMod_authority        },
    { "biotype",          CFeature_table_reader_imp::eOrgMod_biotype          },
    { "biovar",           CFeature_table_reader_imp::eOrgMod_biovar           },
    { "breed",            CFeature_table_reader_imp::eOrgMod_breed            },
    { "chemovar",         CFeature_table_reader_imp::eOrgMod_chemovar         },
    { "common",           CFeature_table_reader_imp::eOrgMod_common           },
    { "cultivar",         CFeature_table_reader_imp::eOrgMod_cultivar         },
    { "dosage",           CFeature_table_reader_imp::eOrgMod_dosage           },
    { "ecotype",          CFeature_table_reader_imp::eOrgMod_ecotype          },
    { "forma_specialis",  CFeature_table_reader_imp::eOrgMod_forma_specialis  },
    { "forma",            CFeature_table_reader_imp::eOrgMod_forma            },
    { "gb_acronym",       CFeature_table_reader_imp::eOrgMod_gb_acronym       },
    { "gb_anamorph",      CFeature_table_reader_imp::eOrgMod_gb_anamorph      },
    { "gb_synonym",       CFeature_table_reader_imp::eOrgMod_gb_synonym       },
    { "group",            CFeature_table_reader_imp::eOrgMod_group            },
    { "isolate",          CFeature_table_reader_imp::eOrgMod_isolate          },
    { "nat_host",         CFeature_table_reader_imp::eOrgMod_nat_host         },
    { "pathovar",         CFeature_table_reader_imp::eOrgMod_pathovar         },
    { "serogroup",        CFeature_table_reader_imp::eOrgMod_serogroup        },
    { "serotype",         CFeature_table_reader_imp::eOrgMod_serotype         },
    { "serovar",          CFeature_table_reader_imp::eOrgMod_serovar          },
    { "specimen_voucher", CFeature_table_reader_imp::eOrgMod_specimen_voucher },
    { "strain",           CFeature_table_reader_imp::eOrgMod_strain           },
    { "sub_species",      CFeature_table_reader_imp::eOrgMod_sub_species      },
    { "subgroup",         CFeature_table_reader_imp::eOrgMod_subgroup         },
    { "substrain",        CFeature_table_reader_imp::eOrgMod_substrain        },
    { "subtype",          CFeature_table_reader_imp::eOrgMod_subtype          },
    { "synonym",          CFeature_table_reader_imp::eOrgMod_synonym          },
    { "teleomorph",       CFeature_table_reader_imp::eOrgMod_teleomorph       },
    { "type",             CFeature_table_reader_imp::eOrgMod_type             },
    { "variety",          CFeature_table_reader_imp::eOrgMod_variety          },
    { "",                 CFeature_table_reader_imp::eOrgMod_bad              }
};

// constructor
CFeature_table_reader_imp::CFeature_table_reader_imp(void)
{
    // initialize common CFeature_table_reader_imp tables
    for (int i = 0; i < sizeof (feat_key_to_subtype) / sizeof (FeatInit); i++) {
        string str = string (feat_key_to_subtype [i].key);
        m_FeatKeys [string (feat_key_to_subtype [i].key)] = feat_key_to_subtype [i].subtype;
    }

    for (int i = 0; i < sizeof (qual_key_to_subtype) / sizeof (QualInit); i++) {
        string str = string (qual_key_to_subtype [i].key);
        m_QualKeys [string (qual_key_to_subtype [i].key)] = qual_key_to_subtype [i].subtype;
    }

    for (int i = 0; i < sizeof (subsrc_key_to_subtype) / sizeof (SubSrcInit); i++) {
        string str = string (subsrc_key_to_subtype [i].key);
        m_SubSrcKeys [string (subsrc_key_to_subtype [i].key)] = subsrc_key_to_subtype [i].subtype;
    }

    for (int i = 0; i < sizeof (orgmod_key_to_subtype) / sizeof (OrgModInit); i++) {
        string str = string (orgmod_key_to_subtype [i].key);
        m_OrgModKeys [string (orgmod_key_to_subtype [i].key)] = orgmod_key_to_subtype [i].subtype;
    }
}

// destructor
CFeature_table_reader_imp::~CFeature_table_reader_imp(void)
{
}


bool CFeature_table_reader_imp::x_ParseFeatureTableLine (const string& line, Int4* startP, Int4* stopP,
                                                         bool* partial5P, bool* partial3P, string& featP,
                                                         string& qualP, string& valP, Int4 offset)

{
    SIZE_TYPE      numtkns;
    bool           badNumber = false;
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
        case eQual_pseudo:
            grp.SetPseudo (true);
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
            return true;
        case eQual_product:
            {
                CProt_ref& prp = sfp->SetProtXref ();
                CProt_ref::TName& prod = prp.SetName ();
                prod.push_back (val);
                return true;
            }
            return true;
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
                case eQual_product:
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


bool CFeature_table_reader_imp::x_AddQualifierToFeature (CRef<CSeq_feat> sfp,
                                                         const string& qual, const string& val)

{
    EQual                  qtyp;
    CSeqFeatData::E_Choice typ;

    if (m_QualKeys.find (qual) != m_QualKeys.end ()) {
        qtyp = m_QualKeys [qual];
        if (qtyp != eQual_bad) {
            CSeqFeatData& sfdata = sfp->SetData ();
            typ = sfdata.Which ();
            switch (typ) {
                case CSeqFeatData::e_Gene:
                    if (x_AddQualifierToGene (sfdata, qtyp, val)) return true;
                    break;
                case CSeqFeatData::e_Cdregion:
                    if (x_AddQualifierToCdregion (sfp, sfdata, qtyp, val)) return true;
                    break;
                case CSeqFeatData::e_Rna:
                    if (x_AddQualifierToRna (sfdata, qtyp, val)) return true;
                    break;
                default:
                   break;
            }
        }
    }
    return true;
}


bool CFeature_table_reader_imp::x_AddIntervalToFeature (CRef<CSeq_feat> sfp, CSeq_loc_mix *mix,
                                                        const string& seqid, Int4 start, Int4 stop,
                                                        bool partial5, bool partial3)

{
    CSeq_interval::TStrand strand = eNa_strand_plus;

    if (start > stop) {
        Int4 flip = start;
        start = stop;
        stop = flip;
        strand = eNa_strand_minus;
    }

    if (start == stop) {
        // just a point
        CRef<CSeq_loc> loc(new CSeq_loc);
        CSeq_point& point = loc->SetPnt();
        point.SetPoint(start);
        point.SetStrand(strand);
        point.SetId().Assign (CSeq_id (seqid));
        mix->Set().push_back(loc);
    } else {
        // interval
        CRef<CSeq_loc> loc(new CSeq_loc);
        CSeq_interval& ival = loc->SetInt();
        ival.SetFrom(start);
        ival.SetTo(stop);
        ival.SetStrand(strand);
        ival.SetId().Assign (CSeq_id (seqid));
        mix->Set().push_back(loc);
    }

    if (partial5 || partial3) {
        sfp->SetPartial (true);
    }

    return true;
}


CRef<CSeq_annot> CFeature_table_reader_imp::ReadSequinFeatureTable (CNcbiIfstream& ifs, const string& seqid, const string& annotname)

{
    string str;
    string feat, qual, val;
    Int4 start, stop;
    bool partial5, partial3;
    Int4 offset = 0;
    CSeqFeatData::ESubtype sbtyp = CSeqFeatData::eSubtype_bad;
    CSeqFeatData::E_Choice typ = CSeqFeatData::e_not_set;
    CRef<CSeq_annot> sap(new CSeq_annot);
    CSeq_annot::C_Data::TFtable& ftable = sap->SetData().SetFtable();
    CRef<CSeq_feat> sfp;
    CSeq_loc_mix *mix = 0;

    while (ifs.good ()) {
        NcbiGetlineEOL (ifs, str);

        if (! str.empty ()) {
            if (str [0] == '>') {

                // skip for now

            } else if (str [0] == '[') {

                // set offset

            } else if (x_ParseFeatureTableLine (str, &start, &stop, &partial5, &partial3, feat, qual, val, offset)) {

                // process line in feature table

                if ((! feat.empty ()) && start >= 0 && stop >= 0) {

                    // process start - stop - feature line

                    if (m_FeatKeys.find (feat) != m_FeatKeys.end ()) {
                        sbtyp = m_FeatKeys [feat];
                        if (sbtyp != CSeqFeatData::eSubtype_bad) {

                            // populate *sfp here...

                            sfp.Reset (new CSeq_feat);
                            sfp->ResetLocation ();

                            typ = CSeqFeatData::GetTypeFromSubtype (sbtyp);
                            sfp->SetData ().Select (typ);
                            CSeqFeatData& sfdata = sfp->SetData ();
                            if (typ == CSeqFeatData::e_Rna) {
                                /* CRef<CRNA_ref> rrp (new CRNA_ref); */
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
                                /* sfp->SetData ().SetRna (*rrp); */
                            }

                            ftable.push_back(sfp);

                            // now create location

                            CRef<CSeq_loc> location (new CSeq_loc);
                            mix = &(location->SetMix ());
                            sfp->SetLocation (*location);

                            // and add first interval

                            x_AddIntervalToFeature (sfp, mix, seqid, start, stop, partial5, partial3);

                        } else {
                            // post error - unrecognized feaure key
                        }
                    } else {
                        // post error - unrecognized feaure key
                    }

                } else if (start >= 0 && stop >= 0 && feat.empty () && qual.empty () && val.empty ()) {

                    // process start - stop multiple interval line

                    x_AddIntervalToFeature (sfp, mix, seqid, start, stop, partial5, partial3);

                } else if ((! qual.empty ()) && (! val.empty ())) {

                    // process qual - val qualifier line

                    x_AddQualifierToFeature (sfp, qual, val);

                }
            }
        }
    }

    // now go through all features and demote single interval seqlocmix to seqlocint

    /*
    for (CTypeConstIterator<CSeq_feat> fi(*sap); fi; ++fi) {
        CSeq_feat& feat = *fi;
        CSeq_loc& location = feat.SetLocation ();
        if (location.IsMix ()) {
            const CSeq_loc_mix& mx = location.GetMix ();
            switch (mx.Get().size()) {
                case 0:
                    location.SetNull();
                    break;
                case 1:
                    location.Reset(mx.Set().front());
                    break;
                default:
                    break;
            }
        }
    }
    */

    return sap;
}

// public access functions

CRef<CSeq_annot> CFeature_table_reader::ReadSequinFeatureTable (CNcbiIfstream& ifs, const string& seqid, const string& annotname)
{
    return x_GetImplementation().ReadSequinFeatureTable (ifs, seqid, annotname);
}

END_objects_SCOPE
END_NCBI_SCOPE
