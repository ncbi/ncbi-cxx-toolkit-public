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

#include <serial/iterator.hpp>
#include <serial/objistrasn.hpp>

// Objects includes
#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Object_id.hpp>
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
        eQual_site_type,
        eQual_standard_name,
        eQual_transcript_id,
        eQual_transl_except,
        eQual_transl_table,
        eQual_translation,
        eQual_transposon,
        eQual_usedin
    };

    enum EOrgRef {
        eOrgRef_organism,
        eOrgRef_organelle,
        eOrgRef_div,
        eOrgRef_lineage,
        eOrgRef_gcode,
        eOrgRef_mgcode
    };

    typedef map< string, CSeqFeatData::ESubtype > TFeatReaderMap;
    typedef map< string, EQual > TQualReaderMap;
    typedef map< string, EOrgRef > TOrgRefReaderMap;
    typedef map< string, CBioSource::EGenome > TGenomeReaderMap;
    typedef map< string, CSubSource::ESubtype > TSubSrcReaderMap;
    typedef map< string, COrgMod::ESubtype > TOrgModReaderMap;
    typedef map< string, CSeqFeatData::EBond > TBondReaderMap;
    typedef map< string, CSeqFeatData::ESite > TSiteReaderMap;
    typedef map< string, int > TTrnaReaderMap;
    typedef vector< string > TSingleQualList;

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
                                 const string& seqid, Int4 start,
                                 Int4 stop, bool partial5, bool partial3);

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

    int x_ParseTrnaString (const string& val);

    TFeatReaderMap    m_FeatKeys;
    TQualReaderMap    m_QualKeys;
    TOrgRefReaderMap  m_OrgRefKeys;
    TGenomeReaderMap  m_GenomeKeys;
    TSubSrcReaderMap  m_SubSrcKeys;
    TOrgModReaderMap  m_OrgModKeys;
    TBondReaderMap    m_BondKeys;
    TSiteReaderMap    m_SiteKeys;
    TTrnaReaderMap    m_TrnaKeys;
    TSingleQualList   m_SingleKeys;
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
    { "operon",             CSeqFeatData::eSubtype_operon             },
    { "oriT",               CSeqFeatData::eSubtype_oriT               },
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
    { "source",             CSeqFeatData::eSubtype_biosrc             },
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
    { "Xref",               CSeqFeatData::eSubtype_seq                }
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
    { "operon",               CFeature_table_reader_imp::eQual_operon               },
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
    { "usedin",               CFeature_table_reader_imp::eQual_usedin               }
};

typedef struct orgrefinit {
    const char *                       key;
    CFeature_table_reader_imp::EOrgRef subtype;
} OrgRefInit;

static OrgRefInit orgref_key_to_subtype [] = {
    { "div",        CFeature_table_reader_imp::eOrgRef_div       },
    { "gcode",      CFeature_table_reader_imp::eOrgRef_gcode     },
    { "lineage",    CFeature_table_reader_imp::eOrgRef_lineage   },
    { "mgcode",     CFeature_table_reader_imp::eOrgRef_mgcode    },
    { "organelle",  CFeature_table_reader_imp::eOrgRef_organelle },
    { "organism",   CFeature_table_reader_imp::eOrgRef_organism  }
};

typedef struct genomeinit {
    const char *        key;
    CBioSource::EGenome subtype;
} GenomeInit;

static GenomeInit genome_key_to_subtype [] = {
    { "unknown",                   CBioSource::eGenome_unknown          },
    { "genomic",                   CBioSource::eGenome_genomic          },
    { "chloroplast",               CBioSource::eGenome_chloroplast      },
    { "plastid:chloroplast",       CBioSource::eGenome_chloroplast      },
    { "chromoplast",               CBioSource::eGenome_chromoplast      },
    { "plastid:chromoplast",       CBioSource::eGenome_chromoplast      },
    { "kinetoplast",               CBioSource::eGenome_kinetoplast      },
    { "mitochondrion:kinetoplast", CBioSource::eGenome_kinetoplast      },
    { "mitochondrion",             CBioSource::eGenome_mitochondrion    },
    { "plastid",                   CBioSource::eGenome_plastid          },
    { "macronuclear",              CBioSource::eGenome_macronuclear     },
    { "extrachrom",                CBioSource::eGenome_extrachrom       },
    { "plasmid",                   CBioSource::eGenome_plasmid          },
    { "transposon",                CBioSource::eGenome_transposon       },
    { "insertion_seq",             CBioSource::eGenome_insertion_seq    },
    { "cyanelle",                  CBioSource::eGenome_cyanelle         },
    { "plastid:cyanelle",          CBioSource::eGenome_cyanelle         },
    { "proviral",                  CBioSource::eGenome_proviral         },
    { "virion",                    CBioSource::eGenome_virion           },
    { "nucleomorph",               CBioSource::eGenome_nucleomorph      },
    { "apicoplast",                CBioSource::eGenome_apicoplast       },
    { "plastid:apicoplast",        CBioSource::eGenome_apicoplast       },
    { "plastid:leucoplast",        CBioSource::eGenome_leucoplast       },
    { "plastid:proplastid",        CBioSource::eGenome_proplastid       },
    { "endogenous_virus",          CBioSource::eGenome_endogenous_virus }
};

typedef struct subsrcinit {
    const char *         key;
    CSubSource::ESubtype subtype;
} SubSrcInit;

static SubSrcInit subsrc_key_to_subtype [] = {
    { "cell_line",            CSubSource::eSubtype_cell_line             },
    { "cell_type",            CSubSource::eSubtype_cell_type             },
    { "chromosome",           CSubSource::eSubtype_chromosome            },
    { "clone",                CSubSource::eSubtype_clone                 },
    { "clone_lib",            CSubSource::eSubtype_clone_lib             },
    { "country",              CSubSource::eSubtype_country               },
    { "dev_stage",            CSubSource::eSubtype_dev_stage             },
    { "endogenous_virus",     CSubSource::eSubtype_endogenous_virus_name },
    { "environmental_sample", CSubSource::eSubtype_environmental_sample  },
    { "frequency",            CSubSource::eSubtype_frequency             },
    { "genotype",             CSubSource::eSubtype_genotype              },
    { "germline",             CSubSource::eSubtype_germline              },
    { "haplotype",            CSubSource::eSubtype_haplotype             },
    { "insertion_seq",        CSubSource::eSubtype_insertion_seq_name    },
    { "isolation_source",     CSubSource::eSubtype_isolation_source      },
    { "lab_host",             CSubSource::eSubtype_lab_host              },
    { "map",                  CSubSource::eSubtype_map                   },
    { "plasmid",              CSubSource::eSubtype_plasmid_name          },
    { "plastid",              CSubSource::eSubtype_plastid_name          },
    { "pop_variant",          CSubSource::eSubtype_pop_variant           },
    { "rearranged",           CSubSource::eSubtype_rearranged            },
    { "segment",              CSubSource::eSubtype_segment               },
    { "sex",                  CSubSource::eSubtype_sex                   },
    { "subclone",             CSubSource::eSubtype_subclone              },
    { "tissue_lib ",          CSubSource::eSubtype_tissue_lib            },
    { "tissue_type",          CSubSource::eSubtype_tissue_type           },
    { "transgenic",           CSubSource::eSubtype_transgenic            },
    { "transposon",           CSubSource::eSubtype_transposon_name       }
};

typedef struct orgmodinit {
    const char *      key;
    COrgMod::ESubtype subtype;
} OrgModInit;

static OrgModInit orgmod_key_to_subtype [] = {
    { "acronym",          COrgMod::eSubtype_acronym          },
    { "anamorph",         COrgMod::eSubtype_anamorph         },
    { "authority",        COrgMod::eSubtype_authority        },
    { "biotype",          COrgMod::eSubtype_biotype          },
    { "biovar",           COrgMod::eSubtype_biovar           },
    { "breed",            COrgMod::eSubtype_breed            },
    { "chemovar",         COrgMod::eSubtype_chemovar         },
    { "common",           COrgMod::eSubtype_common           },
    { "cultivar",         COrgMod::eSubtype_cultivar         },
    { "dosage",           COrgMod::eSubtype_dosage           },
    { "ecotype",          COrgMod::eSubtype_ecotype          },
    { "forma_specialis",  COrgMod::eSubtype_forma_specialis  },
    { "forma",            COrgMod::eSubtype_forma            },
    { "gb_acronym",       COrgMod::eSubtype_gb_acronym       },
    { "gb_anamorph",      COrgMod::eSubtype_gb_anamorph      },
    { "gb_synonym",       COrgMod::eSubtype_gb_synonym       },
    { "group",            COrgMod::eSubtype_group            },
    { "isolate",          COrgMod::eSubtype_isolate          },
    { "nat_host",         COrgMod::eSubtype_nat_host         },
    { "pathovar",         COrgMod::eSubtype_pathovar         },
    { "serogroup",        COrgMod::eSubtype_serogroup        },
    { "serotype",         COrgMod::eSubtype_serotype         },
    { "serovar",          COrgMod::eSubtype_serovar          },
    { "specimen_voucher", COrgMod::eSubtype_specimen_voucher },
    { "strain",           COrgMod::eSubtype_strain           },
    { "sub_species",      COrgMod::eSubtype_sub_species      },
    { "subgroup",         COrgMod::eSubtype_subgroup         },
    { "substrain",        COrgMod::eSubtype_substrain        },
    { "subtype",          COrgMod::eSubtype_subtype          },
    { "synonym",          COrgMod::eSubtype_synonym          },
    { "teleomorph",       COrgMod::eSubtype_teleomorph       },
    { "type",             COrgMod::eSubtype_type             },
    { "variety",          COrgMod::eSubtype_variety          }
};

typedef struct bondinit {
    const char *        key;
    CSeqFeatData::EBond subtype;
} BondInit;

static BondInit bond_key_to_subtype [] = {
    { "disulfide",         CSeqFeatData::eBond_disulfide  },
    { "thiolester",        CSeqFeatData::eBond_thiolester },
    { "xlink",             CSeqFeatData::eBond_xlink      },
    { "thioether",         CSeqFeatData::eBond_thioether  },
    { "",                  CSeqFeatData::eBond_other      }
};

typedef struct siteinit {
    const char *        key;
    CSeqFeatData::ESite subtype;
} SiteInit;

static SiteInit site_key_to_subtype [] = {
    { "active",                      CSeqFeatData::eSite_active                      },
    { "binding",                     CSeqFeatData::eSite_binding                     },
    { "cleavage",                    CSeqFeatData::eSite_cleavage                    },
    { "inhibit",                     CSeqFeatData::eSite_inhibit                     },
    { "modified",                    CSeqFeatData::eSite_modified                    },
    { "glycosylation",               CSeqFeatData::eSite_glycosylation               },
    { "myristoylation",              CSeqFeatData::eSite_myristoylation              },
    { "mutagenized",                 CSeqFeatData::eSite_mutagenized                 },
    { "metal binding",               CSeqFeatData::eSite_metal_binding               },
    { "phosphorylation",             CSeqFeatData::eSite_phosphorylation             },
    { "acetylation",                 CSeqFeatData::eSite_acetylation                 },
    { "amidation",                   CSeqFeatData::eSite_amidation                   },
    { "methylation",                 CSeqFeatData::eSite_methylation                 },
    { "hydroxylation",               CSeqFeatData::eSite_hydroxylation               },
    { "sulfatation",                 CSeqFeatData::eSite_sulfatation                 },
    { "oxidative deamination",       CSeqFeatData::eSite_oxidative_deamination       },
    { "pyrrolidone carboxylic acid", CSeqFeatData::eSite_pyrrolidone_carboxylic_acid },
    { "gamma carboxyglutamic acid",  CSeqFeatData::eSite_gamma_carboxyglutamic_acid  },
    { "blocked",                     CSeqFeatData::eSite_blocked                     },
    { "lipid binding",               CSeqFeatData::eSite_lipid_binding               },
    { "np binding",                  CSeqFeatData::eSite_np_binding                  },
    { "DNA binding",                 CSeqFeatData::eSite_dna_binding                 },
    { "signal peptide",              CSeqFeatData::eSite_signal_peptide              },
    { "transit peptide",             CSeqFeatData::eSite_transit_peptide             },
    { "transmembrane region",        CSeqFeatData::eSite_transmembrane_region        },
    { "",                            CSeqFeatData::eSite_other                       }
};

typedef struct trnainit {
    const char * key;
    int          subtype;
} TrnaInit;

static TrnaInit trna_key_to_subtype [] = {
    { "Ala",   'A' },
    { "Asx",   'B' },
    { "Cys",   'C' },
    { "Asp",   'D' },
    { "Glu",   'E' },
    { "Phe",   'F' },
    { "Gly",   'G' },
    { "His",   'H' },
    { "Ile",   'I' },
    { "Lys",   'K' },
    { "Leu",   'L' },
    { "Met",   'M' },
    { "fMet",  'M' },
    { "Asn",   'N' },
    { "Pro",   'P' },
    { "Gln",   'Q' },
    { "Arg",   'R' },
    { "Ser",   'S' },
    { "Thr",   'T' },
    { "Val",   'V' },
    { "Trp",   'W' },
    { "Xxx",   'X' },
    { "OTHER", 'X' },
    { "Tyr",   'Y' },
    { "Glx",   'Z' },
    { "Sec",   'U' },
    { "Ter",   '*' },
    { "TERM",  '*' }
};

typedef struct singleinit {
    const char * key;
} SingleInit;

static SingleInit single_key_list [] = {
    { "pseudo"               },
    { "germline"             },
    { "rearranged"           },
    { "transgenic"           },
    { "environmental_sample" }
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

    for (int i = 0; i < sizeof (orgref_key_to_subtype) / sizeof (OrgRefInit); i++) {
        string str = string (orgref_key_to_subtype [i].key);
        m_OrgRefKeys [string (orgref_key_to_subtype [i].key)] = orgref_key_to_subtype [i].subtype;
    }

    for (int i = 0; i < sizeof (genome_key_to_subtype) / sizeof (GenomeInit); i++) {
        string str = string (genome_key_to_subtype [i].key);
        m_GenomeKeys [string (genome_key_to_subtype [i].key)] = genome_key_to_subtype [i].subtype;
    }

    for (int i = 0; i < sizeof (subsrc_key_to_subtype) / sizeof (SubSrcInit); i++) {
        string str = string (subsrc_key_to_subtype [i].key);
        m_SubSrcKeys [string (subsrc_key_to_subtype [i].key)] = subsrc_key_to_subtype [i].subtype;
    }

    for (int i = 0; i < sizeof (orgmod_key_to_subtype) / sizeof (OrgModInit); i++) {
        string str = string (orgmod_key_to_subtype [i].key);
        m_OrgModKeys [string (orgmod_key_to_subtype [i].key)] = orgmod_key_to_subtype [i].subtype;
    }

    for (int i = 0; i < sizeof (bond_key_to_subtype) / sizeof (BondInit); i++) {
        string str = string (bond_key_to_subtype [i].key);
        m_BondKeys [string (bond_key_to_subtype [i].key)] = bond_key_to_subtype [i].subtype;
    }

    for (int i = 0; i < sizeof (site_key_to_subtype) / sizeof (SiteInit); i++) {
        string str = string (site_key_to_subtype [i].key);
        m_SiteKeys [string (site_key_to_subtype [i].key)] = site_key_to_subtype [i].subtype;
    }

    for (int i = 0; i < sizeof (trna_key_to_subtype) / sizeof (TrnaInit); i++) {
        string str = string (trna_key_to_subtype [i].key);
        m_TrnaKeys [string (trna_key_to_subtype [i].key)] = trna_key_to_subtype [i].subtype;
    }

    for (int i = 0; i < sizeof (single_key_list) / sizeof (SingleInit); i++) {
        string str = string (single_key_list [i].key);
        m_SingleKeys.push_back (str);
    }
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


int CFeature_table_reader_imp::x_ParseTrnaString (const string& val)

{
    string fst, scd;

    scd = val;
    if (NStr::StartsWith (val, "tRNA-")) {
        NStr::SplitInTwo (val, "-", fst, scd);
    }

    if (m_TrnaKeys.find (scd) != m_TrnaKeys.end ()) {
        return m_TrnaKeys [scd];
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
        case eQual_cons_splice:
        case eQual_direction:
        case eQual_EC_number:
        case eQual_frequency:
        case eQual_function:
        case eQual_insertion_seq:
        case eQual_label:
        case eQual_map:
        case eQual_number:
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
                gbq->SetVal (val);
                qlist.push_back (gbq);
                return true;
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
                if (m_GenomeKeys.find (val) != m_GenomeKeys.end ()) {
                    CBioSource::EGenome gtype = m_GenomeKeys [val];
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

        if (m_OrgRefKeys.find (qual) != m_OrgRefKeys.end ()) {

            EOrgRef rtype = m_OrgRefKeys [qual];
            if (x_AddQualifierToBioSrc (sfdata, rtype, val)) return true;

        } else if (m_SubSrcKeys.find (qual) != m_SubSrcKeys.end ()) {

            CSubSource::ESubtype stype = m_SubSrcKeys [qual];
            if (x_AddQualifierToBioSrc (sfdata, stype, val)) return true;

        } else if (m_OrgModKeys.find (qual) != m_OrgModKeys.end ()) {

            COrgMod::ESubtype  mtype = m_OrgModKeys [qual];
            if (x_AddQualifierToBioSrc (sfdata, mtype, val)) return true;
        }

    } else if (m_QualKeys.find (qual) != m_QualKeys.end ()) {

        EQual qtype = m_QualKeys [qual];
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
                    if (m_BondKeys.find (val) != m_BondKeys.end ()) {
                        CSeqFeatData::EBond btyp = m_BondKeys [val];
                        sfdata.SetBond (btyp);
                        return true;
                    }
                }
                break;
            case CSeqFeatData::e_Site:
                if (qtype == eQual_site_type) {
                    if (m_SiteKeys.find (val) != m_SiteKeys.end ()) {
                        CSeqFeatData::ESite styp = m_SiteKeys [val];
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
            case eQual_cons_splice:
            case eQual_direction:
            case eQual_EC_number:
            case eQual_frequency:
            case eQual_function:
            case eQual_insertion_seq:
            case eQual_label:
            case eQual_map:
            case eQual_number:
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
                    gbq->SetVal (val);
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
                    if (NStr::SplitInTwo (val, ":", db, tag)) {
                        CSeq_feat::TDbxref& dblist = sfp->SetDbxref ();
                        CRef<CDbtag> dbt (new CDbtag);
                        dbt->SetDb (db);
                        CRef<CObject_id> oid (new CObject_id);
                        oid->SetStr (tag);
                        dbt->SetTag (*oid);
                        dblist.push_back (dbt);
                        return true;
                    }
                    return true;
                }
            default:
                break;
        }
    }
    return false;
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
        CSeq_id seq_id(seqid);
        point.SetId().Assign (seq_id);
        mix->Set().push_back(loc);
    } else {
        // interval
        CRef<CSeq_loc> loc(new CSeq_loc);
        CSeq_interval& ival = loc->SetInt();
        ival.SetFrom(start);
        ival.SetTo(stop);
        ival.SetStrand(strand);
        CSeq_id seq_id(seqid);
        ival.SetId().Assign (seq_id);
        mix->Set().push_back(loc);
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

                            ftable.push_back (sfp);

                            // now create location

                            CRef<CSeq_loc> location (new CSeq_loc);
                            mix = &(location->SetMix ());
                            sfp->SetLocation (*location);

                            // and add first interval

                            x_AddIntervalToFeature (sfp, mix, seqid, start, stop, partial5, partial3);

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
                            x_AddIntervalToFeature (sfp, mix, seqid, start, stop, partial5, partial3);
                        }
                    }

                } else if (start >= 0 && stop >= 0 && feat.empty () && qual.empty () && val.empty ()) {

                    // process start - stop multiple interval line

                    x_AddIntervalToFeature (sfp, mix, seqid, start, stop, partial5, partial3);

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
                            gbq->SetVal (val);
                            qlist.push_back (gbq);
                        }
                    }

                } else if ((! qual.empty ()) && (val.empty ())) {

                    // check for the few qualifiers that do not need a value

                    if (find (m_SingleKeys.begin (), m_SingleKeys.end (), qual) != m_SingleKeys.end ()) {

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
        if (m_FeatKeys.find (feat) != m_FeatKeys.end ()) {
            CSeqFeatData::ESubtype sbtyp = m_FeatKeys [feat];
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
                gbq->SetVal (val);
                qlist.push_back (gbq);
            }
        }

    } else if ((! qual.empty ()) && (val.empty ())) {

        // check for the few qualifiers that do not need a value

        if (find (m_SingleKeys.begin (), m_SingleKeys.end (), qual) != m_SingleKeys.end ()) {

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
