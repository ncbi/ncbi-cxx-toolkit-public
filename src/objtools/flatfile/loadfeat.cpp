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
 * File Name: loadfeat.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *      Parse features block to subblock.
 *      Process each subblock.
 *      Output each subblock.
 *      Free out subblock.
 */

#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/RNA_qual_set.hpp>
#include <objects/seqfeat/RNA_qual.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/pub/Pub_set.hpp>
#include <objects/pub/Pub.hpp>
#include <serial/objostr.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objects/seq/seq_loc_from_string.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>

#include "index.h"
#include "embl.h"
#include "genbank.h"
#include "qual_parse.hpp"

#include <objtools/flatfile/flatfile_parser.hpp>
#include <objtools/flatfile/flatdefn.h>

#include "ftaerr.hpp"
#include "indx_blk.h"
#include "asci_blk.h"
#include "utilfeat.h"
#include "loadfeat.h"
#include "add.h"
#include "fta_src.h"
#include "buf_data_loader.h"
#include "utilfun.h"
#include "ref.h"
#include "xgbfeat.h"
#include "xgbparint.h"
#include "fta_xml.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "loadfeat.cpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

#define Seq_descr_GIBB_mol_unknown       CMolInfo::eBiomol_unknown
#define Seq_descr_GIBB_mol_genomic       CMolInfo::eBiomol_genomic
#define Seq_descr_GIBB_mol_preRNA        CMolInfo::eBiomol_pre_RNA
#define Seq_descr_GIBB_mol_mRNA          CMolInfo::eBiomol_mRNA
#define Seq_descr_GIBB_mol_rRNA          CMolInfo::eBiomol_rRNA
#define Seq_descr_GIBB_mol_tRNA          CMolInfo::eBiomol_tRNA
#define Seq_descr_GIBB_mol_uRNA          CMolInfo::eBiomol_snRNA
#define Seq_descr_GIBB_mol_snRNA         CMolInfo::eBiomol_snRNA
#define Seq_descr_GIBB_mol_scRNA         CMolInfo::eBiomol_scRNA
#define Seq_descr_GIBB_mol_other_genetic CMolInfo::eBiomol_other_genetic
#define Seq_descr_GIBB_mol_cRNA          CMolInfo::eBiomol_cRNA
#define Seq_descr_GIBB_mol_snoRNA        CMolInfo::eBiomol_snoRNA
#define Seq_descr_GIBB_mol_trRNA         CMolInfo::eBiomol_transcribed_RNA
#define Seq_descr_GIBB_mol_other         CMolInfo::eBiomol_other

struct TrnaAa {
    const char* name;
    Uint1       aa;
};

struct StrNum {
    const char* str;
    int         num;
};

const TrnaAa taa[] = {
    { "alanine", 'A' },
    { "arginine", 'R' },
    { "asparagine", 'N' },
    { "aspartic acid", 'D' },
    { "aspartate", 'D' },
    { "cysteine", 'C' },
    { "glutamine", 'Q' },
    { "glutamic acid", 'E' },
    { "glutamate", 'E' },
    { "glycine", 'G' },
    { "histidine", 'H' },
    { "isoleucine", 'I' },
    { "leucine", 'L' },
    { "lysine", 'K' },
    { "methionine", 'M' },
    { "phenylalanine", 'F' },
    { "proline", 'P' },
    { "selenocysteine", 'U' },
    { "serine", 'S' },
    { "threonine", 'T' },
    { "tryptophan", 'W' },
    { "tyrosine", 'Y' },
    { "valine", 'V' },
    { nullptr, '\0' }
};

struct AaCodons {
    const char* straa;
    Uint1       intaa;
    Uint1       gencode;
    Int4        vals[8];
};

const AaCodons aacodons[] = {
    { "Ala", 'A', 0, { 52, 53, 54, 55, -1, -1, -1, -1 } },   /* GCT, GCC, GCA, GCG */
    { "Arg", 'R', 2, { 28, 29, 30, 31, -1, -1, -1, -1 } },   /* CGT, CGC, CGA, CGG */
    { "Arg", 'R', 5, { 28, 29, 30, 31, -1, -1, -1, -1 } },   /* CGT, CGC, CGA, CGG */
    { "Arg", 'R', 9, { 28, 29, 30, 31, -1, -1, -1, -1 } },   /* CGT, CGC, CGA, CGG */
    { "Arg", 'R', 13, { 28, 29, 30, 31, -1, -1, -1, -1 } },  /* CGT, CGC, CGA, CGG */
    { "Arg", 'R', 14, { 28, 29, 30, 31, -1, -1, -1, -1 } },  /* CGT, CGC, CGA, CGG */
    { "Arg", 'R', 0, { 28, 29, 30, 31, 46, 47, -1, -1 } },   /* CGT, CGC, CGA, CGG, AGA, AGG */
    { "Asn", 'N', 9, { 40, 41, 42, -1, -1, -1, -1, -1 } },   /* AAT, AAC, AAA */
    { "Asn", 'N', 14, { 40, 41, 42, -1, -1, -1, -1, -1 } },  /* AAT, AAC, AAA */
    { "Asn", 'N', 0, { 40, 41, -1, -1, -1, -1, -1, -1 } },   /* AAT, AAC */
    { "Asp", 'D', 0, { 56, 57, -1, -1, -1, -1, -1, -1 } },   /* GAT, GAC */
    { "Asx", 'B', 9, { 40, 41, 42, 56, 57, -1, -1, -1 } },   /* Asn + Asp */
    { "Asx", 'B', 14, { 40, 41, 42, 56, 57, -1, -1, -1 } },  /* Asn + Asp */
    { "Asx", 'B', 0, { 40, 41, 56, 57, -1, -1, -1, -1 } },   /* Asn + Asp */
    { "Cys", 'C', 10, { 12, 13, 14, -1, -1, -1, -1, -1 } },  /* TGT, TGC, TGA */
    { "Cys", 'C', 0, { 12, 13, -1, -1, -1, -1, -1, -1 } },   /* TGT, TGC */
    { "Gln", 'Q', 6, { 10, 11, 26, 27, -1, -1, -1, -1 } },   /* TAA, TAG, CAA, CAG */
    { "Gln", 'Q', 15, { 11, 26, 27, -1, -1, -1, -1, -1 } },  /* TAG, CAA, CAG */
    { "Gln", 'Q', 0, { 26, 27, -1, -1, -1, -1, -1, -1 } },   /* CAA, CAG */
    { "Glu", 'E', 0, { 58, 59, -1, -1, -1, -1, -1, -1 } },   /* GAA, GAG */
    { "Glx", 'Z', 6, { 10, 11, 26, 27, 58, 59, -1, -1 } },   /* Gln + Glu */
    { "Glx", 'Z', 0, { 11, 26, 27, 58, 59, -1, -1, -1 } },   /* Gln + Glu */
    { "Glx", 'Z', 0, { 26, 27, 58, 59, -1, -1, -1, -1 } },   /* Gln + Glu */
    { "Gly", 'G', 13, { 46, 47, 60, 61, 62, 63, -1, -1 } },  /* AGA, AGG, GGT, GGC, GGA, GGG */
    { "Gly", 'G', 0, { 60, 61, 62, 63, -1, -1, -1, -1 } },   /* GGT, GGC, GGA, GGG */
    { "His", 'H', 0, { 24, 25, -1, -1, -1, -1, -1, -1 } },   /* CAT, CAC */
    { "Ile", 'I', 2, { 32, 33, -1, -1, -1, -1, -1, -1 } },   /* ATT, ATC */
    { "Ile", 'I', 3, { 32, 33, -1, -1, -1, -1, -1, -1 } },   /* ATT, ATC */
    { "Ile", 'I', 5, { 32, 33, -1, -1, -1, -1, -1, -1 } },   /* ATT, ATC */
    { "Ile", 'I', 13, { 32, 33, -1, -1, -1, -1, -1, -1 } },  /* ATT, ATC */
    { "Ile", 'I', 0, { 32, 33, 34, -1, -1, -1, -1, -1 } },   /* ATT, ATC, ATA */
    { "Leu", 'L', 3, { 2, 3, -1, -1, -1, -1, -1, -1 } },     /* TTA, TTG */
    { "Leu", 'L', 12, { 2, 3, 16, 17, 18, -1, -1, -1 } },    /* TTA, TTG, CTT, CTC, CTA */
    { "Leu", 'L', 0, { 2, 3, 16, 17, 18, 19, -1, -1 } },     /* TTA, TTG, CTT, CTC, CTA, CTG */
    { "Lys", 'K', 9, { 43, -1, -1, -1, -1, -1, -1, -1 } },   /* AAG */
    { "Lys", 'K', 14, { 43, -1, -1, -1, -1, -1, -1, -1 } },  /* AAG */
    { "Lys", 'K', 0, { 42, 43, -1, -1, -1, -1, -1, -1 } },   /* AAA, AAG */
    { "Met", 'M', 2, { 34, 35, -1, -1, -1, -1, -1, -1 } },   /* ATA, ATG */
    { "Met", 'M', 3, { 34, 35, -1, -1, -1, -1, -1, -1 } },   /* ATA, ATG */
    { "Met", 'M', 5, { 34, 35, -1, -1, -1, -1, -1, -1 } },   /* ATA, ATG */
    { "Met", 'M', 13, { 34, 35, -1, -1, -1, -1, -1, -1 } },  /* ATA, ATG */
    { "Met", 'M', 0, { 35, -1, -1, -1, -1, -1, -1, -1 } },   /* ATG */
    { "fMet", 'M', 2, { 34, 35, -1, -1, -1, -1, -1, -1 } },  /* ATA, ATG */
    { "fMet", 'M', 3, { 34, 35, -1, -1, -1, -1, -1, -1 } },  /* ATA, ATG */
    { "fMet", 'M', 5, { 34, 35, -1, -1, -1, -1, -1, -1 } },  /* ATA, ATG */
    { "fMet", 'M', 13, { 34, 35, -1, -1, -1, -1, -1, -1 } }, /* ATA, ATG */
    { "fMet", 'M', 0, { 35, -1, -1, -1, -1, -1, -1, -1 } },  /* ATG */
    { "Phe", 'F', 0, { 0, 1, -1, -1, -1, -1, -1, -1 } },     /* TTT, TTC */
    { "Pro", 'P', 0, { 20, 21, 22, 23, -1, -1, -1, -1 } },   /* CCT, CCC, CCA, CCG */
    { "Sec", 'U', 0, { -1, -1, -1, -1, -1, -1, -1, -1 } },
    { "Ser", 'S', 5, { 4, 5, 6, 7, 44, 45, 46, 47 } },       /* TCT, TCC, TCA, TCG, AGT, AGC, AGA, AGG */
    { "Ser", 'S', 9, { 4, 5, 6, 7, 44, 45, 46, 47 } },       /* TCT, TCC, TCA, TCG, AGT, AGC, AGA, AGG */
    { "Ser", 'S', 12, { 4, 5, 6, 7, 19, 44, 45, -1 } },      /* TCT, TCC, TCA, TCG, CTG, AGT, AGC */
    { "Ser", 'S', 14, { 4, 5, 6, 7, 44, 45, 46, 47 } },      /* TCT, TCC, TCA, TCG, AGT, AGC, AGA, AGG */
    { "Ser", 'S', 0, { 4, 5, 6, 7, 44, 45, -1, -1 } },       /* TCT, TCC, TCA, TCG, AGT, AGC */
    { "Thr", 'T', 3, { 16, 17, 18, 19, 36, 37, 38, 39 } },   /* CTT, CTC, CTA, CTG, ACT, ACC, ACA, ACG */
    { "Thr", 'T', 0, { 36, 37, 38, 39, -1, -1, -1, -1 } },   /* ACT, ACC, ACA, ACG */
    { "Trp", 'W', 1, { 15, -1, -1, -1, -1, -1, -1, -1 } },   /* TGG */
    { "Trp", 'W', 6, { 15, -1, -1, -1, -1, -1, -1, -1 } },   /* TGG */
    { "Trp", 'W', 10, { 15, -1, -1, -1, -1, -1, -1, -1 } },  /* TGG */
    { "Trp", 'W', 11, { 15, -1, -1, -1, -1, -1, -1, -1 } },  /* TGG */
    { "Trp", 'W', 12, { 15, -1, -1, -1, -1, -1, -1, -1 } },  /* TGG */
    { "Trp", 'W', 15, { 15, -1, -1, -1, -1, -1, -1, -1 } },  /* TGG */
    { "Trp", 'W', 0, { 14, 15, -1, -1, -1, -1, -1, -1 } },   /* TGA, TGG */
    { "Tyr", 'Y', 14, { 8, 9, 10, -1, -1, -1, -1, -1 } },    /* TAT, TAC, TAA */
    { "Tyr", 'Y', 0, { 8, 9, -1, -1, -1, -1, -1, -1 } },     /* TAT, TAC */
    { "Val", 'V', 0, { 48, 49, 50, 51, -1, -1, -1, -1 } },   /* GTT, GTC, GTA, GTG */
    { "TERM", '*', 1, { 10, 11, 14, -1, -1, -1, -1, -1 } },  /* TAA, TAG, TGA */
    { "TERM", '*', 2, { 10, 11, 46, 47, -1, -1, -1, -1 } },  /* TAA, TAG, AGA, AGG */
    { "TERM", '*', 6, { 14, -1, -1, -1, -1, -1, -1, -1 } },  /* TGA */
    { "TERM", '*', 11, { 10, 11, 14, -1, -1, -1, -1, -1 } }, /* TAA, TAG, TGA */
    { "TERM", '*', 12, { 10, 11, 14, -1, -1, -1, -1, -1 } }, /* TAA, TAG, TGA */
    { "TERM", '*', 14, { 11, -1, -1, -1, -1, -1, -1, -1 } }, /* TAG */
    { "TERM", '*', 15, { 10, 14, -1, -1, -1, -1, -1, -1 } }, /* TAA, TGA */
    { "TERM", '*', 0, { 10, 11, -1, -1, -1, -1, -1, -1 } },  /* TAA, TAG */
    { "OTHER", 'X', 0, { -1, -1, -1, -1, -1, -1, -1, -1 } },
    { nullptr, '\0', 0, { -1, -1, -1, -1, -1, -1, -1, -1 } }
};

static const char* trna_tags[] = {
    "TRANSFERN RNA",
    "TRANSFER RRNA",
    "TRANSFER TRNA",
    "TRANSFER RNA",
    "TRASNFER RNA",
    "TRANSDER RNA",
    "TRANSFERRNA",
    "TRANFER RNA",
    "T RNA",
    "TRNA",
    nullptr
};

const char* ParFlat_ESTmod[] = {
    "EST",
    "expressed sequence tag",
    "partial cDNA sequence",
    "transcribed sequence fragment",
    "TSR",
    "putatively transcribed partial sequence",
    "UK putts",
    "Plastid",
    nullptr
};

static const char* ParFlat_RNA_array[] = {
    "precursor_RNA",
    "mRNA",
    "tRNA",
    "rRNA",
    "snRNA",
    "scRNA",
    "snoRNA",
    "ncRNA",
    "tmRNA",
    "misc_RNA",
    nullptr
};

static const char* DbxrefTagAny[] = {
    "ASAP",
    "CDD",
    "DBEST",
    "DBSTS",
    "GDB",
    "HMP",
    "MAIZEGDB",
    nullptr
};

static const char* DbxrefObsolete[] = {
    "BHB",
    "BIOHEALTHBASE",
    "GENEW",
    "IFO",
    "SWISS-PROT",
    "SPTREMBL",
    "TREMBL",
    nullptr
};

static const char* EMBLDbxrefTagStr[] = {
    "BIOMUTA",
    "DEPOD",
    "ENSEMBLGENOMES-GN",
    "ENSEMBLGENOMES-TR",
    "ESTHER",
    "GENEVISIBLE",
    "MOONPROT",
    "PROTEOMES",
    "UNITE",
    "WBPARASITE",
    nullptr
};

static const char* DbxrefTagStr[] = {
    "ACEVIEW/WORMGENES",
    "APHIDBASE",
    "APIDB",
    "ARAPORT",
    "BEEBASE",
    "BEETLEBASE",
    "BGD",
    "BOLD",
    "CGD",
    "COLLECTF",
    "DBSNP",
    "DICTYBASE",
    "ECOCYC",
    "ECOGENE",
    "ENSEMBL",
    "ENSEMBLGENOMES",
    "ERIC",
    "FANTOM_DB",
    "FLYBASE",
    "GABI",
    "GENEDB",
    "GOA",
    "H-INVDB",
    "HGNC",
    "HOMD",
    "HSSP",
    "I5KNAL",
    "IMGT/GENE-DB",
    "IMGT/HLA",
    "IMGT/LIGM",
    "INTERPRO",
    "IRD",
    "ISD",
    "ISFINDER",
    "ISHAM-ITS",
    "JGIDB",
    "MARPOLBASE",
    "MEDGEN",
    "MGI",
    "MIRBASE",
    "NEXTDB",
    "NIAEST",
    "NMPDR",
    "NRESTDB",
    "OSA1",
    "PATHEMA",
    "PDB",
    "PFAM",
    "PGN",
    "PHYTOZOME",
    "PIR",
    "POMBASE",
    "PSEUDO",
    "PSEUDOCAP",
    "RAP-DB",
    "REMTREMBL",
    "RFAM",
    "RICEGENES",
    "RZPD",
    "SEED",
    "SGD",
    "SGN",
    "SPTREMBL",
    "SRPDB",
    "SUBTILIST",
    "SWISS-PROT",
    "TAIR",
    "TIGRFAM",
    "TREMBL",
    "TUBERCULIST",
    "UNIPROT/SWISS-PROT",
    "UNIPROT/TREMBL",
    "UNIPROTKB/SWISS-PROT",
    "UNIPROTKB/TREMBL",
    "UNITE",
    "VBASE2",
    "VECTORBASE",
    "VGNC",
    "VIPR",
    "VISTA",
    "WORFDB",
    "WORMBASE",
    "XENBASE",
    "ZFIN",
    nullptr
};

static const char* DbxrefTagInt[] = {
    "ATCC",
    "ATCC(DNA)",
    "ATCC(IN HOST)",
    "BDGP_EST",
    "BDGP_INS",
    "ESTLIB",
    "GENEID",
    "GI",
    "GO",
    "GREENGENES",
    "INTREPIDBIO",
    "JCM",
    "LOCUSID",
    "MIM",
    "MYCOBANK",
    "NBRC",
    "PBMICE",
    "RATMAP",
    "RGD",
    "UNILIB",
    "UNISTS",
    nullptr
};

static const char* EmptyQuals[] = {
    "artificial_location", /* Fake. Put here to catch
                                           it's empty */
    "chloroplast",
    "chromoplast",
    "cyanelle",
    "environmental_sample",
    "focus",
    "germline",
    "kinetoplast",
    "macronuclear",
    "metagenomic",
    "mitochondrion",
    "mobile_element_type", /* Fake. Put here to catch
                                           it's empty */
    "partial",
    "proviral",
    "pseudo",
    "rearranged",
    "ribosomal_slippage",
    "trans_splicing",
    "transgenic",
    "virion",
    nullptr
};

const char* TransSplicingFeats[] = {
    "3'UTR",
    "5'UTR",
    "CDS",
    "gene",
    "mRNA",
    "misc_RNA",
    "precursor_RNA",
    "tRNA",
    nullptr
};

const char* ncRNA_class_values[] = {
    "antisense_RNA",
    "autocatalytically_spliced_intron",
    "hammerhead_ribozyme",
    "lncRNA",
    "RNase_P_RNA",
    "RNase_MRP_RNA",
    "telomerase_RNA",
    "guide_RNA",
    "rasiRNA",
    "ribozyme",
    "scRNA",
    "siRNA",
    "miRNA",
    "piRNA",
    "pre_miRNA",
    "snoRNA",
    "snRNA",
    "SRP_RNA",
    "vault_RNA",
    "Y_RNA",
    "other",
    nullptr
};

const char* SatelliteValues[] = {
    "satellite",
    "minisatellite",
    "microsatellite",
    nullptr
};

const char* PseudoGeneValues[] = {
    "allelic",
    "processed",
    "unitary",
    "unknown",
    "unprocessed",
    nullptr
};

const char* RegulatoryClassValues[] = {
    "attenuator",
    "CAAT_signal",
    "DNase_I_hypersensitive_site",
    "enhancer",
    "enhancer_blocking_element",
    "GC_signal",
    "imprinting_control_region",
    "insulator",
    "locus_control_region",
    "matrix_attachment_region",
    "minus_35_signal",
    "minus_10_signal",
    "response_element",
    "polyA_signal_sequence",
    "promoter",
    "recoding_stimulatory_region",
    "replication_regulatory_region",
    "ribosome_binding_site",
    "riboswitch",
    "silencer",
    "TATA_box",
    "terminator",
    "transcriptional_cis_regulatory_region",
    "other",
    nullptr
};

// clang-format off
StrNum GapTypeValues[] = {
    { "between scaffolds",        CSeq_gap::eType_contig },
    { "within scaffold",          CSeq_gap::eType_scaffold },
    { "telomere",                 CSeq_gap::eType_telomere },
    { "centromere",               CSeq_gap::eType_centromere },
    { "short arm",                CSeq_gap::eType_short_arm },
    { "heterochromatin",          CSeq_gap::eType_heterochromatin },
    { "repeat within scaffold",   CSeq_gap::eType_repeat },
    { "repeat between scaffolds", CSeq_gap::eType_repeat },
    { "unknown",                  CSeq_gap::eType_unknown },
    { nullptr, -1 }
};

StrNum LinkageEvidenceValues[] = {
    { "paired-ends",        CLinkage_evidence::eType_paired_ends },
    { "align genus",        CLinkage_evidence::eType_align_genus },
    { "align xgenus",       CLinkage_evidence::eType_align_xgenus },
    { "align trnscpt",      CLinkage_evidence::eType_align_trnscpt },
    { "within clone",       CLinkage_evidence::eType_within_clone },
    { "clone contig",       CLinkage_evidence::eType_clone_contig },
    { "map",                CLinkage_evidence::eType_map },
    { "strobe",             CLinkage_evidence::eType_strobe },
    { "unspecified",        CLinkage_evidence::eType_unspecified },
    { "pcr",                CLinkage_evidence::eType_pcr },
    { "proximity ligation", CLinkage_evidence::eType_proximity_ligation },
    { nullptr, -1 }
};
// clang-format on

void FeatBlk::location_assign(string_view s)
{
    location = StringSave(s);
}

FeatBlk::~FeatBlk()
{
    key.clear();
    if (location) {
        MemFree(location);
        location = nullptr;
    }
}

void DataBlk::SetFeatData(FeatBlk* p)
{
    mpData.ptr = p;
}
FeatBlk* DataBlk::GetFeatData() const
{
    return static_cast<FeatBlk*>(mpData.ptr);
}

extern Int2            SpFeatKeyNameValid(const Char* keystr);
extern CRef<CSeq_feat> SpProcFeatBlk(ParserPtr pp, FeatBlkPtr fbp, TSeqIdList& seqids);

/**********************************************************/
static void FreeFeatBlk(DataBlkPtr dbp, Parser::EFormat format)
{
    DataBlkPtr dbpnext;
    FeatBlkPtr fbp;

    for (; dbp; dbp = dbpnext) {
        dbpnext = dbp->mpNext;
        fbp     = dbp->GetFeatData();
        if (fbp) {
            delete fbp;
            dbp->mpData.ptr = nullptr;
        }
        if (format == Parser::EFormat::XML)
            dbp->SimpleDelete();
    }
}

/**********************************************************
 *
 *   static void DelCharBtwData(value):
 *
 *      Deletes blanks in the "str".
 *
 **********************************************************/
static void DelCharBtwData(char* value)
{
    char* p;

    for (p = value; *p != '\0'; p++)
        if (*p != ' ')
            *value++ = *p;
    *value = '\0';
}

/**********************************************************
 *
 *   static Int4 flat2asn_range_func(pp, sip):
 *
 *      For error handle in gbparint.c routines.
 *      This function has to return the length corresponding
 *   to the SeqId it is passed.
 *
 *                                              ks 1/13/94
 *
 **********************************************************/
static Int4 flat2asn_range_func(void* pp_ptr, const CSeq_id& id)
{
    ParserPtr pp = reinterpret_cast<ParserPtr>(pp_ptr);

    int   use_indx = pp->curindx;

#ifdef BIOSEQ_FIND_METHOD

    bsp = BioseqFind(sip);
    if (bsp)
        return (bsp->length);

    // could try ID0 server
    //
    return (-1);

#else

    const CTextseq_id* text_id = nullptr;
    if (id.IsGenbank() || id.IsEmbl() || id.IsDdbj() || id.IsTpg() ||
        id.IsTpe() || id.IsTpd())
        text_id = id.GetTextseq_Id();

    if (text_id) {
        Int2          text_id_ver = text_id->IsSetVersion() ? text_id->GetVersion() : numeric_limits<short>::min();
        const string& text_id_acc = text_id->GetAccession();
        for (use_indx = 0; use_indx < pp->indx; use_indx++) {
            auto& e = pp->entrylist[use_indx];
            if (text_id_acc == e->acnum &&
                (pp->accver == false || e->vernum == text_id_ver))
                break;
        }

        if (use_indx >= pp->indx) {
            // entry is not present in this file use remote fetch function
            // use_indx = pp->curindx;
            //
            size_t len = pp->ffdb ? CheckOutsideEntry(pp, text_id_acc.c_str(), text_id_ver) : static_cast<size_t>(-1);
            if (len != static_cast<size_t>(-1))
                return static_cast<Int4>(len);

            if (! pp->buf) {
                if (! pp->farseq) {
                    string msg;
                    if (pp->accver == false || text_id_ver < 0)
                        msg = ErrFormat("Location points to outside entry %s", text_id_acc.c_str());
                    else
                        msg = ErrFormat("Location points to outside entry %s.%d", text_id_acc.c_str(), text_id_ver);
                    Nlm_ErrSetContext("validatr", __FILE__, __LINE__);
                    Nlm_ErrPostStr(SEV_WARNING, ERR_LOCATION_FailedCheck, msg);
                }
            } else {
                if (! pp->buf->empty()) {
                    string msg = ErrFormat("Feature location references an interval on another record : %s", pp->buf->c_str());
                    if (pp->source == Parser::ESource::NCBI || pp->source == Parser::ESource::Refseq)
                        ErrPostStr(SEV_WARNING, ERR_LOCATION_NCBIRefersToExternalRecord, msg);
                    else
                        ErrPostStr(SEV_WARNING, ERR_LOCATION_RefersToExternalRecord, msg);
                    pp->buf->clear();
                }
            }
            return (-1);
        }
    }
    return static_cast<Int4>(pp->entrylist[use_indx]->bases);

#endif
}

/**********************************************************/
static bool CheckForeignLoc(const CSeq_loc& loc, const CSeq_id& sid)
{
    const CSeq_id& pid = *loc.GetId();

    if (loc.IsMix() || loc.IsEquiv() ||
        sid.Compare(pid) == CSeq_id::e_YES)
        return false;

    return true;
}

/**********************************************************/
static CRef<CDbtag> DbxrefQualToDbtag(const CGb_qual& qual, Parser::ESource source)
{
    CRef<CDbtag> tag;

    if (! qual.IsSetQual() ||
        qual.GetQual() != "db_xref")
        return tag;

    if (! qual.IsSetVal() || qual.GetVal().empty()) {
        ErrPostStr(SEV_WARNING, ERR_QUALIFIER_EmptyQual, "Found empty /db_xref qualifier. Qualifier dropped.");
        return tag;
    }

    const string& val = qual.GetVal();
    if (NStr::CompareNocase(val.c_str(), "taxon") == 0)
        return tag;

    string line = val;

    if (StringEquNI(line.c_str(), "MGD:MGI:", 8))
        line = line.substr(4);

    size_t colon = line.find(':');
    if (colon == string::npos) {
        auto msg = ErrFormat("Badly formatted /db_xref qualifier: \"%s\". Qualifier dropped.", val.c_str());
        ErrPostStr(SEV_ERROR, ERR_QUALIFIER_DbxrefIncorrect, msg);
        return tag;
    }

    string tail = line.substr(colon + 1);
    line        = line.substr(0, colon);

    if (MatchArrayIString(DbxrefObsolete, line.c_str()) > -1) {
        auto msg = ErrFormat("/db_xref type \"%s\" is obsolete.", line.c_str());
        ErrPostStr(SEV_WARNING, ERR_FEATURE_ObsoleteDbXref, msg);

        string buf;
        if (NStr::CompareNocase(line.c_str(), "BHB") == 0)
            buf = "IRD";
        else if (NStr::CompareNocase(line.c_str(), "BioHealthBase") == 0)
            buf = "IRD";
        else if (NStr::CompareNocase(line.c_str(), "GENEW") == 0)
            buf = "HGNC";
        else if (NStr::CompareNocase(line.c_str(), "IFO") == 0)
            buf = "NBRC";
        else if (NStr::CompareNocase(line.c_str(), "SWISS-PROT") == 0)
            buf = "UniProt/Swiss-Prot";
        else
            buf = "UniProt/TrEMBL";

        line = buf;
    }

    if (NStr::CompareNocase(line.c_str(), "UNIPROT/SWISS-PROT") == 0 ||
        NStr::CompareNocase(line.c_str(), "UNIPROT/TREMBL") == 0) {
        string buf("UniProtKB");
        buf += line.substr(7);

        line = buf;
    }

    const Char* strid = nullptr;
    Int4        intid = 0;

    const Char* p = tail.c_str();
    if (MatchArrayIString(DbxrefTagAny, line.c_str()) > -1) {
        for (strid = p; *p >= '0' && *p <= '9';)
            p++;
        if (*p == '\0' && *strid != '0') {
            intid = atoi(strid);
            strid = nullptr;
        }
    } else if (MatchArrayIString(DbxrefTagStr, line.c_str()) > -1 ||
               (source == Parser::ESource::EMBL &&
                MatchArrayIString(EMBLDbxrefTagStr, line.c_str()) > -1)) {
        for (strid = p; *p >= '0' && *p <= '9';)
            p++;
        if (*p == '\0') {
            auto msg = ErrFormat("/db_xref qualifier \"%s\" is supposed to be a string, but its value consists of digits only.", val.c_str());
            ErrPostStr(SEV_WARNING, ERR_QUALIFIER_DbxrefWrongType, msg);
            if (*strid != '0') {
                intid = atoi(strid);
                strid = nullptr;
            }
        }
    } else if (MatchArrayIString(DbxrefTagInt, line.c_str()) > -1) {
        const Char* q = p;
        for (; *q == '0';)
            q++;
        if (*q == '\0') {
            auto msg = ErrFormat("/db_xref qual should have numeric value greater than 0: \"%s\". Qualifier dropped.", val.c_str());
            ErrPostStr(SEV_ERROR, ERR_QUALIFIER_DbxrefShouldBeNumeric, msg);
            return tag;
        }

        const Char* r = q;
        for (; *r >= '0' && *r <= '9';)
            r++;
        if (*r != '\0') {
            auto msg = ErrFormat("/db_xref qualifier \"%s\" is supposed to be a numeric identifier, but its value includes alphabetic characters. Qualifier dropped.", val.c_str());
            ErrPostStr(SEV_ERROR, ERR_QUALIFIER_DbxrefWrongType, msg);
            return tag;
        }
        if (*r != '\0' || q != p)
            strid = p;
        else if (NStr::CompareNocase(line.c_str(), "IntrepidBio") == 0 && fta_number_is_huge(q))
            strid = q;
        else
            intid = atoi(q);
    } else if (NStr::CompareNocase(line.c_str(), "PID") == 0) {
        if (*p != 'e' && *p != 'g' && *p != 'd') {
            auto msg = ErrFormat("Badly formatted /db_xref qual \"PID\": \"%s\". Qualifier dropped.", val.c_str());
            ErrPostStr(SEV_ERROR, ERR_QUALIFIER_DbxrefIncorrect, msg);
            return tag;
        }

        const Char* q = p + 1;
        for (; *q == '0';)
            q++;

        const Char* r;
        for (r = q; *r >= '0' && *r <= '9';)
            r++;
        if (*q == '\0' || *r != '\0') {
            auto msg = ErrFormat("/db_xref qual \"PID\" should contain numeric value greater than 0: \"%s\". Qualifier dropped.", val.c_str());
            ErrPostStr(SEV_ERROR, ERR_QUALIFIER_DbxrefShouldBeNumeric, msg);
            return tag;
        }
        strid = p;
    } else {
        auto msg = ErrFormat("Unknown data base name /db_xref = \"%s\". Qualifier dropped.", val.c_str());
        ErrPostStr(SEV_ERROR, ERR_QUALIFIER_DbxrefUnknownDBName, msg);
        return tag;
    }


    tag.Reset(new CDbtag);

    tag->SetDb(line);

    if (strid)
        tag->SetTag().SetStr(strid);
    else
        tag->SetTag().SetId(intid);

    return tag;
}

/**********************************************************
 *
 *   Function:
 *      static void FilterDb_xref(pSeqFeat, source)
 *
 *   Purpose:
 *      Looks through SeqFeat's qualifiers which contain
 *      "db_xref" in qual field, convert such qualifiers
 *      into Dbtags removing the qualifiers from SeqFeat's
 *      list, got Dbtags links in the chain of ValNodes
 *      and puts the chain into the SeqFeat.
 *
 *   Parameters:
 *      pSeqFeat - pointer to a SeqFeat for processing
 *
 *   Return:
 *      None.
 *
 **********************************************************/
static void FilterDb_xref(CSeq_feat& feat, Parser::ESource source)
{
    if (! feat.IsSetQual())
        return;

    CSeq_feat::TDbxref& db_refs = feat.SetDbxref();

    for (CSeq_feat::TQual::iterator qual = feat.SetQual().begin(); qual != feat.SetQual().end();) {
        if (! (*qual)->IsSetQual() || (*qual)->GetQual() != "db_xref") {
            /* Just skip this qualifier, it isn't db_xref
             */
            ++qual;
            continue;
        }

        /* Current qualifier is db_xref, process it
         */
        CRef<CDbtag> dbtag = DbxrefQualToDbtag(*(*qual), source);
        if (dbtag.NotEmpty()) {
            db_refs.push_back(dbtag);
        }

        /* Remove converted qualifier from chain of qualifiers
         */
        qual = feat.SetQual().erase(qual);
    }

    if (feat.GetQual().empty())
        feat.ResetQual();

    if (db_refs.empty())
        feat.ResetDbxref();
}

bool GetSeqLocation(CSeq_feat& feat, const char* location, TSeqIdList& ids, bool* hard_err, ParserPtr pp, const string& name)
{
    bool locmap = true;
    int  num_errs;

    *hard_err = false;
    num_errs  = 0;

    CRef<CSeq_loc> loc = xgbparseint_ver(location, locmap, num_errs, ids, pp->accver);

    if (loc.NotEmpty()) {
        TSeqLocList locs;
        locs.push_back(loc);
        fta_fix_seq_loc_id(locs, pp, location, name.c_str(), false);

        feat.SetLocation(*loc);
    }

    if (num_errs > 0) {
        feat.ResetLocation();
        CSeq_loc& cur_loc = feat.SetLocation();
        cur_loc.SetWhole(*(*ids.begin()));
        *hard_err = true;
    } else if (! feat.GetLocation().IsEmpty()) {
        if (feat.GetLocation().IsMix()) {
            if (feat.GetLocation().GetMix().Get().size() == 1) {
                CRef<CSeq_loc> cur_loc(new CSeq_loc);

                cur_loc->Assign(*feat.GetLocation().GetMix().GetFirstLoc());
                if (cur_loc->IsInt())
                    feat.SetLocation(*cur_loc);
            }
        }
    }

    return locmap;
}

/**********************************************************
 *
 *   static char* CheckLocStr(str):
 *
 *      Nlm_gbparseint routine does not parse certain types
 *   of interval correctly, so this routine will save input
 *   form in fbp before passing it:
 *      (bases 100 to 300) ==> 100 to 300;
 *      (bases 1 to 100; 200 to 300) no change.
 *
 *                                              5-20-93
 *
 **********************************************************/
unique_ptr<string> CheckLocStr(const Char* str)
{
    const Char* ptr;
    const Char* eptr;

    ptr = StringChr(str, ';');
    if (ptr)
        return make_unique<string>(str);

    for (ptr = str; *ptr != ' ' && *ptr != '\0';)
        ptr++;
    while (*ptr == ' ')
        ptr++;

    eptr = StringChr(str, ')');
    if (! eptr)
        return {};

    while (*eptr == ' ' || *eptr == ')')
        --eptr;
    ++eptr;

    return make_unique<string>(ptr, eptr);
}

/*****************************************************************************
 *
 *   bool SeqIntCheckCpp(loc) is instead of C-toolkit 'bool SeqIntCheck(sip)'
 *       checks that a seq interval is valid
 *
 *****************************************************************************/
static bool SeqIntCheckCpp(const CSeq_loc& loc)
{
    Uint4 len = numeric_limits<unsigned int>::max();

    CBioseq_Handle bio_h = GetScope().GetBioseqHandle(*loc.GetId());
    if (bio_h.CanGetInst() && bio_h.CanGetInst_Length())
        len = bio_h.GetBioseqLength();

    return loc.GetInt().GetFrom() <= loc.GetInt().GetTo() && loc.GetInt().GetTo() < len;
}

/*****************************************************************************
 *
 *   bool SeqPntCheckCpp(loc) is instead of C-toolkit 'Boolean SeqPntCheck(SeqPntPtr spp)'
 *       checks that a seq point is valid
 *
 *****************************************************************************/
static bool SeqPntCheckCpp(const CSeq_loc& loc)
{
    Uint4 len = numeric_limits<unsigned int>::max();

    CBioseq_Handle bio_h = GetScope().GetBioseqHandle(*loc.GetId());
    if (bio_h.CanGetInst() && bio_h.CanGetInst_Length())
        len = bio_h.GetBioseqLength();

    return loc.GetPnt().GetPoint() < len;
}

/*****************************************************************************
 *
 *   bool PackSeqPntCheck(loc) is instead of C-toolkit 'Boolean PackSeqPntCheck (pspp)'
 *
 *****************************************************************************/
static bool PackSeqPntCheckCpp(const CSeq_loc& loc)
{
    Uint4 len = numeric_limits<unsigned int>::max();

    CBioseq_Handle bio_h = GetScope().GetBioseqHandle(*loc.GetId());
    if (bio_h.CanGetInst() && bio_h.CanGetInst_Length())
        len = bio_h.GetBioseqLength();

    for (TSeqPos point : loc.GetPacked_pnt().GetPoints()) {
        if (point >= len)
            return false;
    }

    return true;
}

/**********************************************************/
/* returns : 2 = Ok, 1 = mixed strands, 0 = error in location
 */
static Uint1 FTASeqLocCheck(const CSeq_loc& locs, char* accession)
{
    Uint1 strand = 99;
    Uint1 retval = 2;

    CSeq_loc_CI ci(locs);

    bool good = true;
    for (; ci; ++ci) {
        CConstRef<CSeq_loc> cur_loc = ci.GetRangeAsSeq_loc();

        const CSeq_id* cur_id = nullptr;

        switch (cur_loc->Which()) {
        case CSeq_loc::e_Int:
            good = SeqIntCheckCpp(*cur_loc);
            if (good)
                cur_id = cur_loc->GetId();
            break;

        case CSeq_loc::e_Pnt:
            good = SeqPntCheckCpp(*cur_loc);
            if (good)
                cur_id = cur_loc->GetId();
            break;

        case CSeq_loc::e_Packed_pnt:
            good = PackSeqPntCheckCpp(*cur_loc);
            if (good)
                cur_id = cur_loc->GetId();
            break;

        case CSeq_loc::e_Bond:
            if (! cur_loc->GetBond().CanGetA())
                good = false;

            if (good)
                cur_id = cur_loc->GetId();
            break;

        case CSeq_loc::e_Empty:
        case CSeq_loc::e_Whole:
            cur_id = cur_loc->GetId();
            break;

        default:
            continue;
        }

        if (! good)
            break;

        if (! accession || ! cur_id)
            continue;

        if (! cur_id->IsGenbank() && ! cur_id->IsEmbl() && ! cur_id->IsPir() &&
            ! cur_id->IsSwissprot() && ! cur_id->IsOther() && ! cur_id->IsDdbj() &&
            ! cur_id->IsPrf() && ! cur_id->IsTpg() && ! cur_id->IsTpe() &&
            ! cur_id->IsTpd() && ! cur_id->IsGpipe())
            continue;

        const CTextseq_id* text_id = cur_id->GetTextseq_Id();

        if (! text_id || ! text_id->CanGetAccession())
            continue;

        if (text_id->GetAccession() == accession) {
            if (strand == 99)
                strand = cur_loc->GetStrand();
            else if (strand != cur_loc->GetStrand())
                retval = 1;
        }
    }

    if (! good)
        return 0;

    return retval;
}

/**********************************************************/
static void fta_strip_aa(char* str)
{
    if (! str || *str == '\0')
        return;

    while (str) {
        str = StringStr(str, "aa");
        if (str)
            fta_StringCpy(str, str + 2);
    }
}

/**********************************************************
 *
 *   static SeqFeatPtr SeqFeatPub(pp, entry, hsfp, seq_id,
 *                                col_data, ibp):
 *
 *                                              5-26-93
 *
 **********************************************************/
static void SeqFeatPub(ParserPtr pp, const DataBlk& entry, TSeqFeatList& feats, TSeqIdList& seqids, Int4 col_data, IndexblkPtr ibp)
{
    DataBlkPtr dbp;
    DataBlkPtr subdbp;
    char*      p;

    bool  err = false;
    Uint1 i;

    /* REFERENCE, to Seq-feat
     */
    if (pp->format == Parser::EFormat::XML)
        dbp = XMLBuildRefDataBlk(entry.mOffset, ibp->xip, ParFlat_REF_BTW);
    else
        dbp = TrackNodeType(entry, ParFlat_REF_BTW);
    if (! dbp)
        return;


    for (; dbp; dbp = dbp->mpNext) {
        if (dbp->mType != ParFlat_REF_BTW)
            continue;

        CRef<CPubdesc> pubdesc = DescrRefs(pp, dbp, col_data);
        if (pubdesc.Empty())
            continue;

        CRef<CSeq_feat> feat(new CSeq_feat);
        feat->SetData().SetPub(*pubdesc);

        unique_ptr<string> ploc;
        if (pp->format == Parser::EFormat::XML) {
            ploc = XMLFindTagValue(dbp->mOffset, dbp->GetXmlData(), INSDREFERENCE_POSITION);
            if (! ploc) {
                auto q = XMLFindTagValue(dbp->mOffset, dbp->GetXmlData(), INSDREFERENCE_REFERENCE);
                if (q) {
                    auto i = q->find('(');
                    if (i != string::npos)
                        ploc = CheckLocStr(&q->operator[](i + 1));
                }
            } else {
                if (ploc->find(';') != string::npos) {
                    string s = "join(";
                    s += *ploc;
                    s += ')';
                    *ploc = s;
                }
            }
        } else if (pp->format == Parser::EFormat::GenBank) {
            for (p = dbp->mOffset + col_data; *p != '\0' && *p != '(';)
                p++;
            ploc = CheckLocStr(string(p, dbp->mOffset + dbp->len - p).c_str());
        } else if (pp->format == Parser::EFormat::EMBL) {
            subdbp = dbp->GetSubData();
            for (; subdbp; subdbp = subdbp->mpNext) {
                if (subdbp->mType != ParFlat_RP)
                    continue;

                for (p = subdbp->mOffset; *p != '\0' && isdigit(*p) == 0;)
                    p++;
                if (StringChr(p, ',')) {
                    string s = "join(";
                    s += p;
                    s += ')';
                    ploc = make_unique<string>(s);
                } else
                    ploc = make_unique<string>(p);
                break;
            }
        }
        if (! ploc || ploc->empty()) {
            ErrPostStr(SEV_REJECT, ERR_REFERENCE_UnparsableLocation, "NULL or empty reference location. Entry dropped.");
            err = true;
            break;
        }

        char* location = StringSave(*ploc);
        if (ibp->is_prot)
            fta_strip_aa(location);

        pp->buf.reset();

        GetSeqLocation(*feat, location, seqids, &err, pp, "pub");

        if (err) {
            ErrPostStr(SEV_REJECT, ERR_REFERENCE_UnparsableLocation, "Unparsable reference location. Entry dropped.");
            MemFree(location);
            break;
        }

        i = FTASeqLocCheck(feat->GetLocation(), ibp->acnum);

        if (i == 0) {
            ErrPostStr(SEV_WARNING, ERR_LOCATION_FailedCheck, location);
            if (pp->debug) {
                feats.push_back(feat);
            }
        } else {
            if (i == 1) {
                auto msg = ErrFormat("Mixed strands in SeqLoc: %s", location);
                ErrPostStr(SEV_WARNING, ERR_LOCATION_MixedStrand, msg);
            }
            feats.push_back(feat);
        }
        if (location)
            MemFree(location);
    }

    if (! err)
        return;

    ibp->drop = true;
    feats.clear();
}

/**********************************************************
 *
 *   static SeqFeatPtr ImpFeatPub(pp, entry, hsfp, seq_id,
 *                                col_data, ibp):
 *
 *                                              5-26-93
 *
 **********************************************************/
static void ImpFeatPub(ParserPtr pp, const DataBlk& entry, TSeqFeatList& feats, CSeq_id& seq_id, Int4 col_data, IndexblkPtr ibp)
{
    DataBlkPtr dbp;

    bool first;

    /* REFERENCE, Imp-feat
     */
    if (pp->format == Parser::EFormat::XML)
        dbp = XMLBuildRefDataBlk(entry.mOffset, ibp->xip, ParFlat_REF_SITES);
    else
        dbp = TrackNodeType(entry, ParFlat_REF_SITES);
    if (! dbp)
        return;

    CRef<CSeq_feat> feat;
    for (first = true; dbp; dbp = dbp->mpNext) {
        if (dbp->mType != ParFlat_REF_SITES)
            continue;

        CRef<CPubdesc> pubdesc = DescrRefs(pp, dbp, col_data);
        if (pubdesc.Empty() || ! pubdesc->IsSetPub())
            continue;

        if (first) {
            feat.Reset(new CSeq_feat);

            CImp_feat& imp_feat = feat->SetData().SetImp();
            imp_feat.SetKey("Site-ref");
            imp_feat.SetLoc("sites");

            feat->SetLocation(*fta_get_seqloc_int_whole(seq_id, ibp->bases));
            first = false;
        }

        CRef<CPub> pub(new CPub);
        pub->SetEquiv(pubdesc->SetPub());

        feat->SetCit().SetPub().push_back(pub);

        if (pubdesc->IsSetComment())
            feat->SetComment(pubdesc->GetComment());
        else
            feat->ResetComment();
    }

    if (! first && feat.NotEmpty())
        feats.push_back(feat);
}

/**********************************************************/
static void fta_fake_gbparse_err_handler(string_view, string_view)
{
}

/**********************************************************/
string location_to_string_or_unknown(const CSeq_loc& loc)
{
    auto ret = location_to_string(loc);
    if (! ret.empty())
        return ret;

    return "unknown location";
}

/**********************************************************/
static CRef<CSeq_loc> GetTrnaAnticodon(const CSeq_feat& feat, char* qval, const TSeqIdList& seqids, bool accver)
{
    char* p;
    char* q;
    bool  fake1;
    Int4  range;
    Int4  pars;
    int   fake3;

    CRef<CSeq_loc> ret;

    if (! qval)
        return ret;

    p = StringStr(qval, "pos:");
    if (! p)
        return ret;

    for (q = p + 4; *q == ' ';)
        q++;

    for (pars = 0, p = q; *p != '\0'; p++) {
        if (*p == ',' && pars == 0)
            break;
        if (*p == '(')
            pars++;
        else if (*p == ')') {
            pars--;
            if (pars == 0) {
                p++;
                break;
            }
        }
    }

    string loc_(q, p);
    const char* loc_str = loc_.c_str();

    xinstall_gbparse_error_handler(fta_fake_gbparse_err_handler);
    ret = xgbparseint_ver(loc_, fake1, fake3, seqids, accver);
    xinstall_gbparse_error_handler(nullptr);

    if (ret.Empty()) {
        string loc = location_to_string_or_unknown(feat.GetLocation());

        auto msg = ErrFormat("Invalid position element for an /anticodon qualifier : \"%s\" : qualifier dropped : feature location \"%s\".", loc_str, (loc.empty()) ? "unknown" : loc.c_str());
        ErrPostStr(SEV_ERROR, ERR_FEATURE_InvalidAnticodonPos, msg);
        return ret;
    }

    range = sequence::GetLength(*ret, &GetScope());
    if (range != 3) {
        string loc = location_to_string_or_unknown(feat.GetLocation());
        if (range == 4) {
            auto msg = ErrFormat("tRNA feature at \"%s\" has anticodon with location spanning four bases: \"%s\". Cannot generate corresponding codon value from the DNA sequence.", loc.empty() ? "unknown" : loc.c_str(), loc_str);
            ErrPostStr(SEV_WARNING, ERR_FEATURE_FourBaseAntiCodon, msg);
        } else {
            auto msg = ErrFormat("tRNA feature at \"%s\" has anticodon of an unusual size: \"%s\". Cannot generate corresponding codon value from the DNA sequence.", loc.empty() ? "unknown" : loc.c_str(), loc_str);
            ErrPostStr(SEV_ERROR, ERR_FEATURE_StrangeAntiCodonSize, msg);
        }
    }

    // Comparing two locations ignoring their IDs
    // Anticodon should be inside the original location (may be the same)
    CRange<TSeqPos> anticodon_range = ret->GetTotalRange();
    CRange<TSeqPos> xrange          = feat.GetLocation().GetTotalRange().IntersectionWith(anticodon_range);

    if (xrange != anticodon_range) {
        string loc = location_to_string_or_unknown(feat.GetLocation());

        auto msg = ErrFormat("Anticodon location \"%s\" does not fall within tRNA feature at \"%s\".", loc_str, loc.empty() ? "unknown" : loc.c_str());
        ErrPostStr(SEV_ERROR, ERR_FEATURE_BadAnticodonLoc, msg);
        ret.Reset();
        return ret;
    }

    return ret;
}

/**********************************************************/
static void fta_parse_rrna_feat(CSeq_feat& feat, CRNA_ref& rna_ref)
{
    char* qval;
    char* p;
    char* q;

    auto qval2 = GetTheQualValue(feat.SetQual(), "product");
    if (feat.GetQual().empty())
        feat.ResetQual();

    string qval_str;
    if (qval2) {
        qval_str = *qval2;
        qval2.reset();
    }

    size_t len = 0;
    if (qval_str.empty() && feat.IsSetComment() && rna_ref.GetType() == CRNA_ref::eType_rRNA) {
        string comment = feat.GetComment();
        len            = comment.size();

        if (len > 15 && len < 20) {
            if (StringEquNI(comment.c_str() + len - 15, "S ribosomal RNA", 15)) {
                qval_str = comment;
                feat.ResetComment();
            }
        } else if (len > 6 && len < 20) {
            if (StringEquNI(comment.c_str() + len - 6, "S rRNA", 6)) {
                qval_str = comment;
                feat.ResetComment();
            }
        }
    }

    if (qval_str.empty())
        return;

    qval = StringSave(qval_str);
    for (p = qval; p; p += 13) {
        p = StringIStr(p, "ribosomal rrna");
        if (! p)
            break;
        fta_StringCpy(p + 10, p + 11);
    }

    for (p = qval; p; p = qval + len) {
        p = StringIStr(p, "ribosomalrna");
        if (! p)
            break;
        p[9] = '\0';
        string s(qval);
        s.append(" RNA");
        s.append(p + 12);
        len = p - qval + 13;
        MemFree(qval);
        qval = StringSave(s);
    }

    if (qval) {
        p = StringIStr(qval, " rrna");
        if (p) {
            *p = '\0';
            string s(qval);
            s.append(" ribosomal RNA");
            s.append(p + 5);
            MemFree(qval);
            qval = StringSave(s);
        }
    }

    for (p = qval, q = p; q; q = p + 13) {
        p = StringIStr(q, "ribosomal DNA");
        if (! p) {
            p = StringIStr(q, "ribosomal RNA");
            if (! p)
                break;
        }
        p[10] = 'R';
        p[11] = 'N';
        p[12] = 'A';
    }

    p = StringIStr(qval, "s ribosomal RNA");
    if (p && p > qval && p[15] == '\0') {
        p--;
        if (*p >= '0' && *p <= '9')
            *++p = 'S';
    }

    for (p = qval;;) {
        p = StringIStr(p, "ribosomal");
        if (! p)
            break;
        if (p == qval || (p[9] != ' ' && p[9] != '\0')) {
            p += 9;
            continue;
        }
        if (StringEquN(p + 9, " RNA", 4)) {
            p += 13;
            continue;
        }
        len = p - qval + 14;
        p += 9;
        string s(qval, p);
        s.append(" RNA");
        s.append(p);
        MemFree(qval);
        qval = StringSave(s);
        p    = qval + len;
    }

    for (p = qval;;) {
        p = StringIStr(p, " ribosomal RNA");
        if (! p)
            break;
        p += 14;
        if (StringEquNI(p, " ribosomal RNA", 14))
            fta_StringCpy(p, p + 14);
    }

    DeleteQual(feat.SetQual(), "product");
    if (feat.GetQual().empty())
        feat.ResetQual();

    if (StringLen(qval) > 511) {
        qval[510] = '>';
        qval[511] = '\0';
        p         = StringSave(qval);
        MemFree(qval);
        qval = p;
    }

    rna_ref.SetExt().SetName(qval);
    MemFree(qval);
}

/**********************************************************/
static Uint1 fta_get_aa_from_symbol(Char ch)
{
    const AaCodons* acp;

    for (acp = aacodons; acp->straa; acp++)
        if (acp->intaa == ch)
            break;
    if (acp->straa)
        return (acp->intaa);

    return (0);
}

/**********************************************************/
static Uint1 fta_get_aa_from_string(char* str)
{
    const AaCodons* acp;
    const TrnaAa*   tap;

    for (tap = taa; tap->name; tap++)
        if (NStr::CompareNocase(str, tap->name) == 0)
            break;
    if (tap->name)
        return (tap->aa);

    for (acp = aacodons; acp->straa; acp++)
        if (NStr::CompareNocase(acp->straa, str) == 0)
            break;
    if (acp->straa)
        return (acp->intaa);

    return (0);
}

/**********************************************************/
static int get_aa_from_trna(const CTrna_ext& trna)
{
    int ret = 0;
    if (trna.IsSetAa() && trna.GetAa().IsNcbieaa())
        ret = trna.GetAa().GetNcbieaa();

    return ret;
}

/**********************************************************/
static CRef<CTrna_ext> fta_get_trna_from_product(CSeq_feat& feat, const string& product, unsigned char* remove)
{
    const char** b;

    char* p;
    char* q;
    char* start;
    char* end;
    char* first;
    char* second;
    char* third;
    char* fourth;
    bool  fmet;
    char* prod;

    if (remove)
        *remove = 0;

    CRef<CTrna_ext> ret(new CTrna_ext);

    if (product.length() < 7)
        return ret;

    bool digits = false;
    prod        = StringSave(product);
    for (p = prod; *p != '\0'; p++) {
        if (*p >= 'a' && *p <= 'z')
            *p &= ~040;
        else if ((*p < 'A' || *p > 'Z') && *p != '(' && *p != ')') {
            if (*p >= '0' && *p <= '9')
                digits = true;
            *p = ' ';
        }
    }
    ShrinkSpaces(prod);

    for (b = trna_tags; *b; b++) {
        start = StringStr(prod, *b);
        if (start)
            break;
    }
    if (! *b) {
        MemFree(prod);
        return ret;
    }

    end = start + StringLen(*b);
    for (p = end; *p != '\0'; p++)
        if (*p == '(' || *p == ')')
            *p = ' ';
    ShrinkSpaces(prod);

    if (start == prod && *end == '\0') {
        if (remove && ! digits)
            *remove = 1;
        MemFree(prod);
        return ret;
    }

    first  = nullptr;
    second = nullptr;
    third  = nullptr;
    fourth = nullptr;
    for (p = end; *p == ' ' || *p == ')' || *p == '(';)
        p++;
    q = p;
    if (StringEquN(p, "F MET", 5))
        p += 5;
    else if (StringEquN(p, "F MT", 4))
        p += 4;
    while (*p >= 'A' && *p <= 'Z')
        p++;
    if (p > q) {
        if (*p != '\0')
            *p++ = '\0';
        second = q;
    }
    while (*p == ' ' || *p == ')' || *p == '(')
        p++;
    for (q = p; *p >= 'A' && *p <= 'Z';)
        p++;
    if (p > q) {
        if (*p != '\0')
            *p++ = '\0';
        if (q[1] == '\0') {
            while (*p == ' ' || *p == ')' || *p == '(')
                p++;
            for (q = p; *p >= 'A' && *p <= 'Z';)
                p++;
            if (p > q) {
                if (*p != '\0')
                    *p++ = '\0';
                third = q;
            }
        } else
            third = q;

        while (*p == ' ' || *p == '(' || *p == ')')
            p++;
        if (*p != '\0')
            fourth = p;
    }
    if (start > prod) {
        for (p = start - 1; *p == ' ' || *p == ')' || *p == '('; p--)
            if (p == prod)
                break;

        if (p > prod && p[1] == ')') {
            for (p--; *p != '('; p--)
                if (p == prod)
                    break;
            if (p > prod) {
                for (p--; *p == ' ' || *p == '(' || *p == '('; p--)
                    if (p == prod)
                        break;
            }
        }
        if (p > prod) {
            for (q = p++; *q >= 'A' && *q <= 'Z'; q--)
                if (q == prod)
                    break;
            if (*q < 'A' || *q > 'Z')
                q++;
            if (p > q) {
                *p    = '\0';
                first = q;
            }
        }
    }

    fmet = false;
    if (second) {
        if (StringEqu(second, "F MET") ||
            StringEqu(second, "FMET") ||
            StringEqu(second, "F MT")) {
            StringCpy(second, "FMET");
            fmet = true;
        }

        ret->SetAa().SetNcbieaa(fta_get_aa_from_string(second));
        if (get_aa_from_trna(*ret) != 0)
            second = nullptr;
    }

    if (get_aa_from_trna(*ret) == 0 && first) {
        ret->SetAa().SetNcbieaa(fta_get_aa_from_string(first));
        if (get_aa_from_trna(*ret) != 0 && first == prod)
            first = nullptr;
    }

    if (! first && ! second && ! third && ! fourth && remove && ! digits)
        *remove = 1;
    MemFree(prod);

    if (! fmet)
        return ret;

    if (! feat.IsSetComment())
        feat.SetComment("fMet");
    else if (! StringIStr(feat.GetComment().c_str(), "fmet")) {
        string& comment = feat.SetComment();
        comment += "; fMet";
    }

    return ret;
}

/**********************************************************/
static CRef<CTrna_ext> fta_get_trna_from_comment(const string& comment, unsigned char* remove)
{
    char* comm;
    char* p;
    char* q;

    CRef<CTrna_ext> ret(new CTrna_ext);

    *remove = 0;
    if (comment.empty())
        return ret;

    comm = StringSave(comment);
    for (p = comm; *p != '\0'; p++) {
        if (*p >= 'a' && *p <= 'z')
            *p &= ~040;
        else if (*p < 'A' || *p > 'Z')
            *p = ' ';
    }
    ShrinkSpaces(comm);

    if (StringEquN(comm, "CODON RECOGNIZED ", 17)) {
        p = comm + 17;
        q = StringChr(p, ' ');
        if (q && StringEqu(q + 1, "PUTATIVE"))
            *q = '\0';
        if (! StringChr(p, ' ') && StringLen(p) == 3) {
            MemFree(comm);
            *remove = q ? 2 : 1;
            return ret;
        }
    }

    if (StringEquN(comm, "PUTATIVE ", 9) && comm[10] == ' ' &&
        comm[14] == ' ' && StringEquN(&comm[15], "TRNA", 4)) {
        ret->SetAa().SetNcbieaa(fta_get_aa_from_symbol(comm[9]));
        if (get_aa_from_trna(*ret) != 0) {
            MemFree(comm);
            return ret;
        }
    }

    for (q = comm, p = q; p;) {
        p = StringChr(p, ' ');
        if (p)
            *p++ = '\0';

        ret->SetAa().SetNcbieaa(fta_get_aa_from_string(q));
        if (get_aa_from_trna(*ret) != 0)
            break;
        q = p;
    }

    MemFree(comm);
    return ret;
}

/**********************************************************/
static int get_first_codon_from_trna(const CTrna_ext& trna)
{
    int ret = 255;
    if (trna.IsSetCodon() && ! trna.GetCodon().empty())
        ret = *trna.GetCodon().begin();

    return ret;
}

/**********************************************************/
static void GetRnaRef(CSeq_feat& feat, CBioseq& bioseq, Parser::ESource source, bool accver)
{
    optional<string> qval;

    Uint1 remove;

    Int2 type;

    if (! feat.GetData().IsImp())
        return;

    const CImp_feat& imp_feat = feat.GetData().GetImp();

    CRef<CRNA_ref> rna_ref(new CRNA_ref);

    type = MatchArrayString(ParFlat_RNA_array, imp_feat.GetKey().c_str());
    if (type < 0)
        type = 255;
    else
        ++type;

    rna_ref->SetType(static_cast<CRNA_ref::EType>(type));

    feat.SetData().SetRna(*rna_ref);

    if (type == CRNA_ref::eType_rRNA) {
        fta_parse_rrna_feat(feat, *rna_ref);
        return;
    }

    CRef<CRNA_gen>      rna_gen;
    CRef<CRNA_qual_set> rna_quals;

    if (type == CRNA_ref::eType_ncRNA) {
        auto p = GetTheQualValue(feat.SetQual(), "ncRNA_class");
        if (p) {
            rna_gen.Reset(new CRNA_gen);
            rna_gen->SetClass(*p);
        }
    } else if (type == CRNA_ref::eType_tmRNA) {
        auto p = GetTheQualValue(feat.SetQual(), "tag_peptide");
        if (p) {
            CRef<CRNA_qual> rna_qual(new CRNA_qual);
            rna_qual->SetQual("tag_peptide");
            rna_qual->SetVal(*p);

            rna_quals.Reset(new CRNA_qual_set);
            rna_quals->Set().push_back(rna_qual);

            rna_gen.Reset(new CRNA_gen);
            rna_gen->SetQuals(*rna_quals);
        }
    }

    if (type != CRNA_ref::eType_premsg && type != CRNA_ref::eType_tRNA) /* mRNA, snRNA, scRNA or other */
    {
        qval = GetTheQualValue(feat.SetQual(), "product"); // may return newly allocated memory!!!
        if (qval) {
            auto p = GetTheQualValue(feat.SetQual(), "product");
            if (p && ! p->empty()) {
                if (! feat.IsSetComment())
                    feat.SetComment(*p);
                else {
                    string& comment = feat.SetComment();
                    comment += "; ";
                    comment += *p;
                }
            }
        }

        if (! qval && type == CRNA_ref::eType_mRNA &&
            source != Parser::ESource::EMBL && source != Parser::ESource::DDBJ)
            qval = GetTheQualValue(feat.SetQual(), "standard_name");

        if (! qval && feat.IsSetComment() && type == CRNA_ref::eType_mRNA) {
            const Char* c_p = feat.GetComment().c_str();
            const Char* c_q = nullptr;
            for (;; c_p += 5, c_q = c_p) {
                c_p = StringIStr(c_p, " mRNA");
                if (! c_p)
                    break;
            }

            const Char* c_r = nullptr;
            for (c_p = feat.GetComment().c_str();; c_p += 4, c_r = c_p) {
                c_p = StringIStr(c_p, " RNA");
                if (! c_p)
                    break;
            }

            if (c_q && c_r) {
                c_p = (c_q > c_r) ? c_q : c_r;
            } else if (c_q)
                c_p = c_q;
            else
                c_p = c_r;

            if (c_p) {
                while (*c_p == ' ' || *c_p == '\t' || *c_p == ',' || *c_p == ';')
                    ++c_p;

                if (*c_p == '\0') {
                    qval = feat.GetComment();
                    feat.ResetComment();
                }
            }
        }

        if (qval) {
            if (qval->length() > 511) {
                qval->resize(511);
                qval->back() = '>';
            }

            if (type > CRNA_ref::eType_snoRNA && type <= CRNA_ref::eType_miscRNA) {
                if (rna_gen.Empty())
                    rna_gen.Reset(new CRNA_gen);

                rna_gen->SetProduct(*qval);
            } else {
                rna_ref->SetExt().SetName(*qval);
            }
        }
        qval.reset();
    }

    if (feat.GetQual().empty())
        feat.ResetQual();

    if (rna_gen.NotEmpty()) {
        rna_ref->SetExt().SetGen(*rna_gen);
    }

    if (type != CRNA_ref::eType_tRNA) /* if tRNA and codon value exist */
        return;

    if (qval) {
        qval.reset();
    }
    qval = GetTheQualValue(feat.SetQual(), "anticodon");
    CRef<CTrna_ext> trnaa;
    if (qval) {
        bioseq.SetInst().SetMol(CSeq_inst::eMol_na);

        CRef<CSeq_loc> anticodon = GetTrnaAnticodon(feat, qval->data(), bioseq.GetId(), accver);
        if (anticodon.NotEmpty()) {
            trnaa.Reset(new CTrna_ext);

            /* value has format: (pos:base_range, aa:amino_acid)
             */
            trnaa->SetAa().SetNcbieaa(GetQualValueAa(qval->c_str(), true));
            trnaa->SetAnticodon(*anticodon);
            rna_ref->SetExt().SetTRNA(*trnaa);
        }
        qval.reset();
    }

    string qval2 = CpTheQualValue(feat.SetQual(), "product");

    CRef<CTrna_ext> trnap;
    if (! qval2.empty()) {
        trnap = fta_get_trna_from_product(feat, qval2, nullptr);
        qval2.clear();
    }

    if (feat.IsSetComment() && feat.GetComment().empty()) {
        feat.ResetComment();
    }

    remove = 0;
    CRef<CTrna_ext> trnac;
    if (feat.IsSetComment()) {
        trnac = fta_get_trna_from_product(feat, feat.GetComment(), &remove);

        if (get_aa_from_trna(*trnac) == 0) {
            trnac = fta_get_trna_from_comment(feat.GetComment(), &remove);
        }

        if (get_aa_from_trna(*trnac) == 0 && get_first_codon_from_trna(*trnac) == 255) {
            trnac.Reset();
        }
    }

    if (trnaa.Empty()) {
        if (trnap.Empty()) {
            if (trnac.NotEmpty() && get_aa_from_trna(*trnac) != 0) {
                rna_ref->SetExt().SetTRNA(*trnac);
                if (remove != 0) {
                    feat.ResetComment();
                }
            }
        } else {
            rna_ref->SetExt().SetTRNA(*trnap);

            if (get_aa_from_trna(*trnap) == 0) {
                if (trnac.NotEmpty() && get_aa_from_trna(*trnac) != 0)
                    rna_ref->SetExt().SetTRNA(*trnac);
            } else if (trnac.NotEmpty()) {
                if (get_aa_from_trna(*trnac) == 0 && get_first_codon_from_trna(*trnac) != 255 &&
                    get_first_codon_from_trna(*trnap) == 255 && remove != 0) {
                    trnap->SetCodon().assign(trnac->GetCodon().begin(), trnac->GetCodon().end());

                    feat.ResetComment();
                    if (remove == 2)
                        feat.SetComment("putative");
                }

                if (get_aa_from_trna(*trnac) == get_aa_from_trna(*trnap) && remove != 0) {
                    feat.ResetComment();
                }
            }
        }
    } else {
        if (trnap.NotEmpty()) {
            trnap.Reset();
        }

        if (trnac.NotEmpty() && get_aa_from_trna(*trnac) != 0) {
            if (get_aa_from_trna(*trnac) == get_aa_from_trna(*trnaa) || get_aa_from_trna(*trnaa) == 88) {
                trnac->SetAnticodon(trnaa->SetAnticodon());
                trnaa->ResetAnticodon();

                if (get_first_codon_from_trna(*trnac) == 255) {
                    trnac->SetCodon().assign(trnaa->GetCodon().begin(), trnaa->GetCodon().end());
                }

                rna_ref->SetExt().SetTRNA(*trnac);
                if (remove != 0) {
                    feat.ResetComment();
                }
            }
        }
    }

    if (feat.GetQual().empty())
        feat.ResetQual();

    if (rna_ref->IsSetExt() && rna_ref->GetExt().IsTRNA()) {
        const CTrna_ext& trna = rna_ref->GetExt().GetTRNA();
        if (get_aa_from_trna(trna) == 0 && ! trna.IsSetAnticodon()) {
            rna_ref->ResetExt();
        }
    }
}

/**********************************************************
 *
 *   static void GetImpFeat(sfp, fbp, locmap):
 *
 *      'replace' in loc will be changed later
 *   in SeqEntryToAsn3Ex.
 *
 *                                              01/07/97
 *
 **********************************************************/
static void GetImpFeat(CSeq_feat& feat, FeatBlkPtr fbp, bool locmap)
{
    CRef<CImp_feat> imp_feat(new CImp_feat);
    imp_feat->SetKey(fbp->key);

    if (locmap)
        imp_feat->SetLoc(fbp->location_get());

    feat.SetData().SetImp(*imp_feat);
}

/**********************************************************/
void fta_sort_biosource(CBioSource& bio)
{
    if (bio.CanGetOrg() && ! bio.GetOrg().GetDb().empty()) {
        for (COrg_ref::TDb::iterator db = bio.SetOrg().SetDb().begin(); db != bio.SetOrg().SetDb().end(); ++db) {
            if (! (*db)->CanGetDb())
                continue;

            COrg_ref::TDb::iterator tdb = db;
            for (++tdb; tdb != bio.SetOrg().SetDb().end(); ++tdb) {
                if (! (*tdb)->IsSetDb())
                    continue;

                if ((*db)->GetDb() < (*tdb)->GetDb())
                    continue;

                if ((*db)->GetDb() == (*tdb)->GetDb()) {
                    const CObject_id& db_id  = (*db)->GetTag();
                    const CObject_id& tdb_id = (*tdb)->GetTag();

                    if (! db_id.IsStr() && tdb_id.IsStr())
                        continue;

                    if (db_id.IsStr() && tdb_id.IsStr() &&
                        db_id.GetStr() <= tdb_id.GetStr())
                        continue;

                    if (! db_id.IsStr() && ! tdb_id.IsStr() &&
                        db_id.GetId() <= tdb_id.GetId())
                        continue;
                }

                db->Swap(*tdb);
            }
        }

        if (bio.GetOrg().IsSetOrgname() && bio.GetOrg().GetOrgname().IsSetMod()) {
            COrgName::TMod& rmod = bio.SetOrg().SetOrgname().SetMod();
            for (COrgName::TMod::iterator mod = rmod.begin(); mod != rmod.end(); ++mod) {
                COrgName::TMod::iterator tmod = mod;
                for (++tmod; tmod != rmod.end(); ++tmod) {
                    if ((*mod)->GetSubtype() < (*tmod)->GetSubtype())
                        continue;

                    if ((*mod)->GetSubtype() == (*tmod)->GetSubtype() &&
                        (*mod)->GetSubname() <= (*tmod)->GetSubname())
                        continue;

                    mod->Swap(*tmod);
                }
            }
        }
    }

    if (! bio.IsSetSubtype())
        return;

    CBioSource::TSubtype& rsub = bio.SetSubtype();
    for (CBioSource::TSubtype::iterator sub = rsub.begin(); sub != rsub.end(); ++sub) {
        CBioSource::TSubtype::iterator tsub = sub;
        for (++tsub; tsub != rsub.end(); ++tsub) {
            if ((*sub)->GetSubtype() < (*tsub)->GetSubtype())
                continue;

            if ((*sub)->GetSubtype() == (*tsub)->GetSubtype() &&
                (*sub)->GetName() <= (*tsub)->GetName())
                continue;

            sub->Swap(*tsub);
        }
    }
}

/**********************************************************/
static void ConvertQualifierValue(CRef<CGb_qual>& qual)
{
    string val       = qual->GetVal();
    bool   has_comma = val.find(',') != string::npos;

    if (has_comma) {
        std::replace(val.begin(), val.end(), ',', ';');
        qual->SetVal(val);
    }

    if (has_comma)
        ErrPostStr(SEV_WARNING, ERR_QUALIFIER_MultRptUnitComma, "Converting commas to semi-colons due to format conventions for multiple /rpt_unit qualifiers.");
}

/**********************************************************/
static void fta_parse_rpt_units(FeatBlkPtr fbp)
{
    if (! fbp || fbp->quals.empty())
        return;

    TQualVector::iterator first = fbp->quals.end();
    size_t                len = 0, count = 0;

    for (TQualVector::iterator qual = fbp->quals.begin(); qual != fbp->quals.end();) {
        if ((*qual)->GetQual() != "rpt_unit") {
            ++qual;
            continue;
        }

        auto msg = ErrFormat("Obsolete /rpt_unit qualifier found on feature \"%s\" at location \"%s\".", fbp->key_or("Unknown"), fbp->location_or("unknown"));
        ErrPostStr(SEV_ERROR, ERR_QUALIFIER_ObsoleteRptUnit, msg);

        if ((*qual)->GetVal().empty()) {
            qual = fbp->quals.erase(qual);
            continue;
        }

        count++;
        len += (*qual)->GetVal().size();
        if (first == fbp->quals.end())
            first = qual;

        if (count == 1) {
            ++qual;
            continue;
        }

        if (count == 2)
            ConvertQualifierValue(*first);

        ConvertQualifierValue(*qual);
        ++qual;
    }

    if (count == 0)
        return;

    if (count == 1) {
        const string& val = (*first)->GetVal();
        if (*val.begin() == '(' && *val.rbegin() == ')') {
            ConvertQualifierValue(*first);
        }
        return;
    }

    string p;
    p.reserve(len + count + 1);
    p.assign("(");
    p.append((*first)->GetVal());

    for (TQualVector::iterator qual = first; qual != fbp->quals.end();) {
        if ((*qual)->GetQual() != "rpt_unit") {
            ++qual;
            continue;
        }

        p.append(",");
        p.append((*qual)->GetVal());
        qual = fbp->quals.erase(qual);
    }
    p.append(")");
    (*first)->SetVal(p);
}

/**********************************************************/
static bool fta_check_evidence(CSeq_feat& feat, FeatBlkPtr fbp)
{
    Int4 evi_exp;
    Int4 evi_not;
    Int4 exp_good;
    Int4 exp_bad;
    Int4 inf_good;
    Int4 inf_bad;

    if (! fbp || fbp->quals.empty())
        return true;

    evi_exp  = 0;
    evi_not  = 0;
    exp_good = 0;
    exp_bad  = 0;
    inf_good = 0;
    inf_bad  = 0;

    for (TQualVector::iterator qual = fbp->quals.begin(); qual != fbp->quals.end();) {
        const string& qual_str = (*qual)->IsSetQual() ? (*qual)->GetQual() : "";
        const string& val_str  = (*qual)->IsSetVal() ? (*qual)->GetVal() : "";
        if (qual_str == "experiment") {
            if (val_str == "experimental evidence, no additional details recorded") {
                exp_good++;
                qual = fbp->quals.erase(qual);
            } else {
                exp_bad++;
                ++qual;
            }
            continue;
        }

        if (qual_str == "inference") {
            if (val_str == "non-experimental evidence, no additional details recorded") {
                inf_good++;
                qual = fbp->quals.erase(qual);
            } else {
                inf_bad++;
                ++qual;
            }
            continue;
        }

        if (qual_str != "evidence") {
            ++qual;
            continue;
        }

        if (NStr::CompareNocase(val_str.c_str(), "not_experimental") == 0)
            evi_not++;
        else if (NStr::CompareNocase(val_str.c_str(), "experimental") == 0)
            evi_exp++;
        else {
            auto msg = ErrFormat("Illegal value \"%s\" for /evidence qualifier on the \"%s\" feature at \"%s\". Qualifier dropped.", val_str.empty() ? "Unknown" : val_str.c_str(), fbp->key_or("Unknown"), fbp->location_or("unknown location"));
            ErrPostStr(SEV_ERROR, ERR_QUALIFIER_InvalidEvidence, msg);
        }

        qual = fbp->quals.erase(qual);
    }

    if (evi_exp + evi_not > 0 && exp_good + exp_bad + inf_good + inf_bad > 0) {
        auto msg = ErrFormat("Old /evidence and new /experiment or /inference qualifiers both exist on the \"%s\" feature at \"%s\". This is currently unsupported.", fbp->key_or("Unknown"), fbp->location_or("unknown location"));
        ErrPostStr(SEV_REJECT, ERR_QUALIFIER_Conflict, msg);
        return false;
    }

    if (evi_exp + exp_good > 0 && evi_not + inf_good > 0) {
        auto msg = ErrFormat("The special \"no additional details recorded\" values for both /experiment and /inference exist on the \"%s\" feature at \"%s\". This is currently unsupported.", fbp->key_or("Unknown"), fbp->location_or("unknown location"));
        ErrPostStr(SEV_REJECT, ERR_QUALIFIER_Conflict, msg);
        return false;
    }

    if ((exp_good > 0 && exp_bad > 0) || (inf_good > 0 && inf_bad > 0)) {
        auto msg = ErrFormat("The special \"no additional details recorded\" value for /experiment or /inference exists in conjunction with other /experiment or /inference qualifiers on the \"%s\" feature at \"%s\". This is currently unsupported.", fbp->key_or("Unknown"), fbp->location_or("unknown location"));
        ErrPostStr(SEV_REJECT, ERR_QUALIFIER_Conflict, msg);
        return false;
    }

    if (exp_good + evi_exp > 0)
        feat.SetExp_ev(CSeq_feat::eExp_ev_experimental);
    else if (inf_good + evi_not > 0)
        feat.SetExp_ev(CSeq_feat::eExp_ev_not_experimental);
    return true;
}

/**********************************************************
 *
 *   static CRef<CSeq_feat> ProcFeatBlk(pp, fbp, seqids):
 *
 *      Process each feature sub-block.
 *      location, SeqLocPtr by calling Karl's routine,
 *   Nml_gbparseint which return locmap = TRUE if mapping
 *   location rules not work, then SeqLocPtr->whole = seqids[0].
 *   sitesmap = TRUE if found "(sites" string, num_errs > 0
 *   if any errors occurred.
 *      If there is a illegal location, then assign
 *   qualifier to be a Imp-feat.
 *
 **********************************************************/
static CRef<CSeq_feat> ProcFeatBlk(ParserPtr pp, FeatBlkPtr fbp, TSeqIdList& seqids)
{
    const char** b;

    char* loc = nullptr;

    bool locmap = false;
    bool err    = false;

    CRef<CSeq_feat> feat;

    if (fbp->location_isset()) {
        loc = fbp->location;
        DelCharBtwData(loc);
        pp->buf = fbp->key + " : " + loc;
        feat.Reset(new CSeq_feat);
        locmap = GetSeqLocation(*feat, loc, seqids, &err, pp, fbp->key);
        pp->buf.reset();
    }
    if (err) {
        if (pp->debug == false) {
            auto msg = ErrFormat("%s|%s| range check detects problems", fbp->key.c_str(), loc);
            ErrPostStr(SEV_ERROR, ERR_FEATURE_Dropped, msg);
            feat.Reset();
            return feat;
        }
        auto msg = ErrFormat("%s|%s| range check detects problems", fbp->key.c_str(), loc);
        ErrPostStr(SEV_WARNING, ERR_LOCATION_FailedCheck, msg);
    }

    if (! fbp->quals.empty()) {
        if (DeleteQual(fbp->quals, "partial"))
            feat->SetPartial(true);
    }

    if (StringStr(loc, "order"))
        feat->SetPartial(true);

    if (! fbp->quals.empty()) {
        if (DeleteQual(fbp->quals, "pseudo"))
            feat->SetPseudo(true);
    }

    if (! fbp->quals.empty())
        DeleteQual(fbp->quals, "gsdb_id");

    if (! fbp->quals.empty())
        fta_parse_rpt_units(fbp);

    if (! fbp->quals.empty()) {
        for (b = TransSplicingFeats; *b; b++)
            if (fbp->key == *b)
                break;
        if (*b && DeleteQual(fbp->quals, "trans_splicing")) {
            feat->SetExcept(true);
            if (! feat->IsSetExcept_text())
                feat->SetExcept_text("trans-splicing");
            else {
                string& exc_text = feat->SetExcept_text();
                exc_text += ", trans-splicing";
            }
        }
    }

    if (! fta_check_evidence(*feat, fbp)) {
        pp->entrylist[pp->curindx]->drop = true;
        return feat;
    }

    if ((! feat->IsSetPartial() || ! feat->GetPartial()) && fbp->key != "gap") {
        if (SeqLocHaveFuzz(feat->GetLocation()))
            feat->SetPartial(true);
    }

    if (! fbp->quals.empty()) {
        auto comment = GetTheQualValue(fbp->quals, "note");
        if (comment) {
            if (! comment->empty()) {
                feat->SetComment(*comment);
            }
        }
    }

    /* assume all imp for now
     */
    if (fbp->key.find("source") == string::npos)
        GetImpFeat(*feat, fbp, locmap);

    for (const auto& cur : fbp->quals) {
        const string& qual_str = cur->GetQual();
        if (qual_str == "pseudogene")
            feat->SetPseudo(true);

        // Do nothing for 'translation' qualifier in case of its value is empty
        if (qual_str == "translation" && (! cur->IsSetVal() || cur->GetVal().empty()))
            continue;

        if (! qual_str.empty())
            feat->SetQual().push_back(cur);
    }

    return feat;
}

/**********************************************************/
static void fta_get_gcode_from_biosource(const CBioSource& bio_src, IndexblkPtr ibp)
{
    if (! bio_src.IsSetOrg() || ! bio_src.GetOrg().IsSetOrgname())
        return;

    ibp->gc_genomic = bio_src.GetOrg().GetOrgname().IsSetGcode() ? bio_src.GetOrg().GetOrgname().GetGcode() : 0;
    ibp->gc_mito    = bio_src.GetOrg().GetOrgname().IsSetMgcode() ? bio_src.GetOrg().GetOrgname().GetMgcode() : 0;
}

/**********************************************************/
static void fta_sort_quals(FeatBlkPtr fbp, bool qamode)
{
    if (! fbp)
        return;

    for (TQualVector::iterator q = fbp->quals.begin(); q != fbp->quals.end(); ++q) {
        if ((*q)->GetQual() == "gene" ||
            (! qamode && (*q)->GetQual() == "product"))
            continue;

        TQualVector::iterator tq = q;
        for (++tq; tq != fbp->quals.end(); ++tq) {
            const string& q_qual  = (*q)->GetQual();
            const string& tq_qual = (*tq)->GetQual();

            if (! tq_qual.empty()) {
                if (q_qual == "gene")
                    continue;

                Int4 i = NStr::CompareNocase(q_qual.c_str(), tq_qual.c_str());
                if (i < 0)
                    continue;
                if (i == 0) {
                    /* Do not sort /gene qualifiers
                     */
                    const string q_val  = (*q)->GetVal();
                    const string tq_val = (*tq)->GetVal();

                    if (q_val.empty())
                        continue;

                    if (! tq_val.empty()) {
                        if (q_val[0] >= '0' && q_val[0] <= '9' &&
                            tq_val[0] >= '0' && tq_val[0] <= '9') {
                            if (atoi(q_val.c_str()) <= atoi(tq_val.c_str()))
                                continue;
                        } else if (q_val <= tq_val)
                            continue;
                    }
                }
            }

            q->Swap(*tq);
        }
    }
}

/**********************************************************/
static bool fta_qual_a_in_b(const TQualVector& qual1, const TQualVector& qual2)
{
    bool found = false;

    for (const auto& gbqp1 : qual1) {
        found = false;
        for (const auto& gbqp2 : qual2) {
            const Char* qual_a = gbqp1->IsSetQual() ? gbqp1->GetQual().c_str() : nullptr;
            const Char* qual_b = gbqp2->IsSetQual() ? gbqp2->GetQual().c_str() : nullptr;

            const Char* val_a = gbqp1->IsSetVal() ? gbqp1->GetVal().c_str() : nullptr;
            const Char* val_b = gbqp2->IsSetVal() ? gbqp2->GetVal().c_str() : nullptr;

            if (fta_strings_same(qual_a, qual_b) && fta_strings_same(val_a, val_b)) {
                found = true;
                break;
            }
        }
        if (! found)
            break;
    }

    if (! found)
        return false;

    return true;
}

/**********************************************************/
static bool fta_feats_same(const FeatBlk* fbp1, const FeatBlk* fbp2)
{
    if (! fbp1 && ! fbp2)
        return true;
    if (! fbp1 || ! fbp2 ||
        ! fta_strings_same(fbp1->key.c_str(), fbp2->key.c_str()) ||
        ! fta_strings_same(fbp1->location_c_str(), fbp2->location_c_str()))
        return false;

    if (fta_qual_a_in_b(fbp1->quals, fbp2->quals) && fta_qual_a_in_b(fbp2->quals, fbp1->quals))
        return true;

    return false;
}

/**********************************************************/
static bool fta_check_rpt_unit_span(const char* val, size_t length)
{
    const char* p;
    const char* q;
    Int4        i1;
    Int4        i2;

    if (! val || *val == '\0')
        return false;

    for (p = val; *p >= '0' && *p <= '9';)
        p++;

    if (p == val || p[0] != '.' || p[1] != '.')
        return false;

    i1 = atoi(val);
    for (p += 2, q = p; *q >= '0' && *q <= '9';)
        q++;
    if (q == p || *q != '\0')
        return false;
    i2 = atoi(p);

    if (i1 == 0 || i1 > i2 || i2 > (Int4)length)
        return false;
    return true;
}

/**********************************************************/
static void fta_check_rpt_unit_range(FeatBlkPtr fbp, size_t length)
{
    if (! fbp || fbp->quals.empty())
        return;

    for (TQualVector::iterator cur = fbp->quals.begin(); cur != fbp->quals.end();) {
        if (! (*cur)->IsSetQual() || ! (*cur)->IsSetVal()) {
            ++cur;
            continue;
        }

        const string& qual_str = (*cur)->GetQual();
        const string& val_str  = (*cur)->GetVal();

        if (qual_str != "rpt_unit_range" || fta_check_rpt_unit_span(val_str.c_str(), length)) {
            ++cur;
            continue;
        }

        string _loc = fbp->location_or("unknown");
        if (_loc.size() > 20) {
            _loc.resize(20);
            _loc += "...";
        }
        auto msg = ErrFormat("/rpt_unit_range qualifier \"%s\" on feature \"%s\" at location \"%s\" is not a valid basepair range. Qualifier dropped.", val_str.empty() ? "(EMPTY)" : val_str.c_str(), fbp->key_or("Unknown"), _loc.c_str());
        ErrPostStr(SEV_ERROR, ERR_QUALIFIER_InvalidRptUnitRange, msg);

        cur = fbp->quals.erase(cur);
    }
}

/**********************************************************/
static void fta_remove_dup_feats(DataBlkPtr dbp)
{
    DataBlkPtr tdbp;
    DataBlkPtr tdbpprev;
    DataBlkPtr tdbpnext;
    const FeatBlk* fbp1;
    const FeatBlk* fbp2;

    if (! dbp || ! dbp->mpNext)
        return;

    for (; dbp; dbp = dbp->mpNext) {
        if (! dbp->hasData())
            continue;

        fbp1     = dbp->GetFeatData();
        tdbpprev = dbp;
        for (tdbp = dbp->mpNext; tdbp; tdbp = tdbpnext) {
            tdbpnext = tdbp->mpNext;
            if (! tdbp->hasData()) {
                tdbpprev->mpNext = tdbpnext;
                tdbp->SimpleDelete();
                continue;
            }

            fbp2 = tdbp->GetFeatData();

            if (fbp1->location_isset() && fbp2->location_isset() &&
                StringCmp(fbp1->location_get(), fbp2->location_get()) < 0)
                break;

            if (! fta_feats_same(fbp1, fbp2)) {
                tdbpprev = tdbp;
                continue;
            }

            string _loc = fbp2->location_or("???");
            if (_loc.size() > 20) {
                _loc.resize(20);
                _loc += "...";
            }
            auto msg = ErrFormat("Duplicated feature \"%s\" at location \"%s\" removed.", fbp2->key_or("???"), _loc.c_str());
            ErrPostStr(SEV_WARNING, ERR_FEATURE_DuplicateRemoved, msg);

            delete fbp2;
            tdbpprev->mpNext = tdbpnext;
            tdbp->SimpleDelete();
        }
    }
}

/**********************************************************/
class PredIsGivenQual
{
public:
    PredIsGivenQual(const string& qual) :
        qual_(qual) {}

    bool operator()(const CRef<CGb_qual>& qual)
    {
        return qual->GetQual() == qual_;
    }

private:
    string qual_;
};

static void fta_check_multiple_locus_tag(DataBlkPtr dbp, bool* drop)
{
    FeatBlkPtr fbp;

    for (; dbp; dbp = dbp->mpNext) {
        fbp = dbp->GetFeatData();
        if (! fbp)
            continue;

        size_t i = std::count_if(fbp->quals.begin(), fbp->quals.end(), PredIsGivenQual("locus_tag"));
        if (i < 2)
            continue;

        auto msg = ErrFormat("Multiple /locus_tag values for \"%s\" feature at \"%s\".", fbp->key_or("Unknown"), fbp->location_or("unknown location"));
        ErrPostStr(SEV_REJECT, ERR_FEATURE_MultipleLocusTags, msg);
        *drop = true;
        break;
    }
}

/**********************************************************/
static void fta_check_old_locus_tags(DataBlkPtr dbp, bool* drop)
{
    PredIsGivenQual isOldLocusTag("old_locus_tag"),
        isLocusTag("locus_tag");

    for (; dbp; dbp = dbp->mpNext) {
        FeatBlkPtr fbp = dbp->GetFeatData();
        if (! fbp)
            continue;
        size_t olt = std::count_if(fbp->quals.begin(), fbp->quals.end(), isOldLocusTag);
        size_t lt  = std::count_if(fbp->quals.begin(), fbp->quals.end(), isLocusTag);

        if (olt == 0)
            continue;

        if (lt == 0) {
            auto msg = ErrFormat("Feature \"%s\" at \"%s\" has an /old_locus_tag qualifier but lacks a /locus_tag qualifier. Entry dropped.", fbp->key_or("Unknown"), fbp->location_or("unknown location"));
            ErrPostStr(SEV_REJECT, ERR_FEATURE_OldLocusTagWithoutNew, msg);
            *drop = true;
        } else {
            for (const auto& gbqp1 : fbp->quals) {
                if (! gbqp1->IsSetQual() || ! gbqp1->IsSetVal() || ! isLocusTag(gbqp1))
                    continue;

                const string& gbqp1_val = gbqp1->GetVal();
                if (gbqp1_val.empty())
                    continue;

                for (const auto& gbqp2 : fbp->quals) {
                    if (! gbqp2->IsSetQual() || ! gbqp2->IsSetVal())
                        continue;

                    const string& gbqp2_val = gbqp2->GetVal();

                    if (! isOldLocusTag(gbqp2) || ! NStr::EqualNocase(gbqp1_val, gbqp2_val))
                        continue;
                    auto msg = ErrFormat("Feature \"%s\" at \"%s\" has an /old_locus_tag qualifier with a value that is identical to that of a /locus_tag qualifier: \"%s\". Entry dropped.", fbp->key_or("Unknown"), fbp->location_or("unknown location"), gbqp1_val.c_str());
                    ErrPostStr(SEV_REJECT, ERR_FEATURE_MatchingOldNewLocusTag, msg);
                    *drop = true;
                }
            }
        }

        if (olt == 1)
            continue;

        for (TQualVector::const_iterator gbqp1 = fbp->quals.begin(); gbqp1 != fbp->quals.end(); ++gbqp1) {
            const string& gbqp1_val = (*gbqp1)->GetVal();
            if (isOldLocusTag(*gbqp1) || gbqp1_val.empty())
                continue;

            TQualVector::const_iterator gbqp2 = gbqp1;
            for (++gbqp2; gbqp2 != fbp->quals.end(); ++gbqp2) {
                const string& gbqp2_val = (*gbqp2)->GetVal();
                if (isOldLocusTag(*gbqp2) || gbqp2_val.empty())
                    continue;

                if (NStr::CompareNocase(gbqp1_val.c_str(), gbqp2_val.c_str()) == 0) {
                    auto msg = ErrFormat("Feature \"%s\" at \"%s\" has redundant /old_locus_tag qualifiers. Dropping all but the first.", fbp->key_or("Unknown"), fbp->location_or("unknown location"));
                    ErrPostStr(SEV_ERROR, ERR_FEATURE_RedundantOldLocusTag, msg);
                    break;
                }
            }

            if (gbqp2 != fbp->quals.end())
                break;
        }
    }
}

/**********************************************************/
static void fta_check_pseudogene_qual(DataBlkPtr dbp)
{
    FeatBlkPtr fbp;
    bool       got_pseudogene;
    bool       got_pseudo;

    for (; dbp; dbp = dbp->mpNext) {
        fbp = dbp->GetFeatData();
        if (! fbp)
            continue;

        got_pseudo     = false;
        got_pseudogene = false;

        for (TQualVector::iterator cur = fbp->quals.begin(); cur != fbp->quals.end();) {
            const string& qual_str = (*cur)->GetQual();
            const string& val_str  = (*cur)->IsSetVal() ? (*cur)->GetVal() : "";

            if (qual_str != "pseudogene") {
                if (! got_pseudo && qual_str == "pseudo")
                    got_pseudo = true;
                ++cur;
                continue;
            }

            if (got_pseudogene) {
                auto msg = ErrFormat("Dropping a /pseudogene qualifier because multiple /pseudogene qualifiers are present : <%s> : Feature key <%s> : Feature location <%s>.", val_str.empty() ? "[empty]" : val_str.c_str(), fbp->key.c_str(), fbp->location_c_str());
                ErrPostStr(SEV_ERROR, ERR_QUALIFIER_MultiplePseudoGeneQuals, msg);

                cur = fbp->quals.erase(cur);
                continue;
            }

            got_pseudogene = true;

            if (val_str.empty()) {
                auto msg = ErrFormat("Dropping a /pseudogene qualifier because its value is empty : Feature key <%s> : Feature location <%s>.", fbp->key.c_str(), fbp->location_c_str());
                ErrPostStr(SEV_ERROR, ERR_QUALIFIER_InvalidPseudoGeneValue, msg);

                cur = fbp->quals.erase(cur);
                continue;
            }

            if (MatchArrayString(PseudoGeneValues, val_str.c_str()) >= 0) {
                ++cur;
                continue;
            }

            auto msg = ErrFormat("Dropping a /pseudogene qualifier because its value is invalid : <%s> : Feature key <%s> : Feature location <%s>.", val_str.c_str(), fbp->key.c_str(), fbp->location_c_str());
            ErrPostStr(SEV_ERROR, ERR_QUALIFIER_InvalidPseudoGeneValue, msg);

            cur = fbp->quals.erase(cur);
        }

        if (! got_pseudogene || ! got_pseudo)
            continue;

        auto msg = ErrFormat("A legacy /pseudo qualifier and a /pseudogene qualifier are present on the same feature : Dropping /pseudo : Feature key <%s> : Feature location <%s>.", fbp->key.c_str(), fbp->location_c_str());
        ErrPostStr(SEV_ERROR, ERR_QUALIFIER_OldPseudoWithPseudoGene, msg);
        DeleteQual(fbp->quals, "pseudo");
    }
}

/**********************************************************/
static void fta_check_compare_qual(DataBlkPtr dbp, bool is_tpa)
{
    FeatBlkPtr fbp;
    Int4       com_count;
    Int4       cit_count;

    for (; dbp; dbp = dbp->mpNext) {
        fbp = dbp->GetFeatData();
        if (! fbp)
            continue;

        com_count = 0;
        cit_count = 0;

        for (TQualVector::iterator cur = fbp->quals.begin(); cur != fbp->quals.end();) {
            const string& qual_str = (*cur)->GetQual();
            const string dummy;
            const string& val_str  = (*cur)->IsSetVal() ? (*cur)->SetVal() : dummy;

            if (qual_str == "compare") {
                bool badcom = true;
                if (! val_str.empty()) {
                    const char* q = StringChr(val_str.c_str(), '.');
                    if (q && q[1] != '\0') {
                        const char* p;
                        for (p = q + 1; *p >= '0' && *p <= '9';)
                            p++;
                        if (*p == '\0') {
                            if (GetNucAccOwner(CTempString(val_str, 0, q - val_str.c_str())) > CSeq_id::e_not_set)
                                badcom = false;
                        }
                    }
                }

                if (badcom) {
                    auto msg = ErrFormat("/compare qualifier value is not a legal Accession.Version : feature \"%s\" at \"%s\" : value \"%s\" : qualifier has been dropped.", fbp->key.c_str(), fbp->location_c_str(), val_str.empty() ? "[empty]" : val_str.c_str());
                    ErrPostStr(SEV_ERROR, ERR_QUALIFIER_IllegalCompareQualifier, msg);

                    cur = fbp->quals.erase(cur);
                    continue;
                }
                com_count++;
            } else if (qual_str == "citation")
                cit_count++;

            ++cur;
        }

        if (com_count > 0 || cit_count > 0 ||
            (fbp->key != "old_sequence" && fbp->key != "conflict"))
            continue;

        auto msg = ErrFormat("Feature \"%s\" at \"%s\" lacks required /citation and/or /compare qualifier : feature has been dropped.", fbp->key.c_str(), fbp->location_c_str());
        ErrPostStr(SEV_ERROR, ERR_FEATURE_RequiredQualifierMissing, msg);
        dbp->mDrop = true;
    }
}

/**********************************************************/
static void fta_check_non_tpa_tsa_tls_locations(DataBlkPtr  dbp,
                                                IndexblkPtr ibp)
{
    FeatBlkPtr fbp;
    char*      location;
    char*      p;
    char*      q;
    char*      r;
    Uint1      i;

    location = nullptr;
    for (; dbp; dbp = dbp->mpNext) {
        fbp = dbp->GetFeatData();
        if (! fbp || ! fbp->location_isset())
            continue;
        location = StringSave(fbp->location_get());
        for (p = location, q = p; *p != '\0'; p++)
            if (*p != ' ' && *p != '\t' && *p != '\n')
                *q++ = *p;
        *q = '\0';
        if (q == location) {
            MemFree(location);
            location = nullptr;
            continue;
        }

        for (p = location + 1; *p != '\0'; p++) {
            if (*p != ':')
                continue;
            for (r = nullptr, q = p - 1;; q--) {
                if (q == location) {
                    if (*q != '_' && (*q < '0' || *q > '9') &&
                        (*q < 'a' || *q > 'z') && (*q < 'A' || *q > 'Z'))
                        q++;
                    break;
                }
                if (*q == '.') {
                    if (! r) {
                        r = q;
                        continue;
                    }
                    q++;
                    break;
                }
                if (*q != '_' && (*q < '0' || *q > '9') &&
                    (*q < 'a' || *q > 'z') && (*q < 'A' || *q > 'Z')) {
                    q++;
                    break;
                }
            }
            if (q == p)
                continue;
            i = GetNucAccOwner(CTempString(q, (r ? r : p) - q));
            if (i == CSeq_id::e_Genbank && (q[0] == 'e' || q[0] == 'E') &&
                (q[1] == 'z' || q[1] == 'Z') && ibp->is_tpa == false)
                continue;
            if (ibp->is_tpa && (i == CSeq_id::e_Tpg || i == CSeq_id::e_Tpd ||
                                i == CSeq_id::e_Tpe))
                continue;
            break;
        }
        if (*p != '\0')
            break;
        if (location) {
            MemFree(location);
            location = nullptr;
        }
    }
    if (! dbp)
        return;

    ibp->drop = true;
    if (location && StringLen(location) > 45) {
        location[40] = '\0';
        StringCat(location, "...");
    }
    if (ibp->is_tsa) {
        auto msg = ErrFormat("Feature \"%s\" at \"%s\" on a TSA record cannot point to a non-TSA record.", fbp->key.c_str(), location ? location : "empty_location");
        ErrPostStr(SEV_REJECT, ERR_LOCATION_AccessionNotTSA, msg);
    } else if (ibp->is_tls) {
        auto msg = ErrFormat("Feature \"%s\" at \"%s\" on a TLS record cannot point to a non-TLS record.", fbp->key.c_str(), location ? location : "empty_location");
        ErrPostStr(SEV_REJECT, ERR_LOCATION_AccessionNotTLS, msg);
    } else {
        auto msg = ErrFormat("Feature \"%s\" at \"%s\" on a TPA record cannot point to a non-TPA record.", fbp->key.c_str(), location ? location : "empty_location");
        ErrPostStr(SEV_REJECT, ERR_LOCATION_AccessionNotTPA, msg);
    }
    if (location)
        MemFree(location);
}

/**********************************************************/
static bool fta_perform_operon_checks(TSeqFeatList& feats, IndexblkPtr ibp)
{
    using FTAOperonList = list<FTAOperon*>;
    FTAOperonList operonList;
    FTAOperonList residentList;
    bool          success = true;

    if (feats.empty()) {
        return true;
    }

    for (const auto& pFeat : feats) {
        if (! pFeat->GetData().IsImp())
            continue;

        const auto&      featLocation = pFeat->GetLocation();
        const CImp_feat& featImp      = pFeat->GetData().GetImp();
        FTAOperon*       pLatest(nullptr);
        int              opQualCount(0);

        for (const auto& pQual : pFeat->GetQual()) {
            const auto& qual = *pQual;
            if (! qual.IsSetQual() || qual.GetQual() != "operon" ||
                ! qual.IsSetVal() || qual.GetVal().empty()) {
                continue;
            }
            opQualCount++;

            pLatest = new FTAOperon(
                featImp.IsSetKey() ? featImp.GetKey().c_str() : "Unknown",
                qual.GetVal(),
                featLocation);
            if (pLatest->IsOperon()) {
                operonList.push_back(pLatest);
            } else {
                residentList.push_back(pLatest);
                continue;
            }
            for (const auto& operon : operonList) {
                if (pLatest == operon) {
                    continue;
                }
                if (pLatest->mOperon != operon->mOperon) {
                    continue;
                }
                auto msg = ErrFormat("The operon features at \"%s\" and \"%s\" utilize the same /operon qualifier : \"%s\".", operon->LocationStr().c_str(), pLatest->LocationStr().c_str(), pLatest->mOperon.c_str());
                ErrPostStr(SEV_REJECT, ERR_FEATURE_OperonQualsNotUnique, msg);
                success = false;
            }
        }

        if (opQualCount > 1) {
            auto msg = ErrFormat("Feature \"%s\" at \"%s\" has more than one operon qualifier.", pLatest->mFeatname.c_str(), pLatest->LocationStr().c_str());
            ErrPostStr(SEV_REJECT, ERR_FEATURE_MultipleOperonQuals, msg);
            success = false;
        }

        if (opQualCount == 0 && featImp.IsSetKey() && featImp.GetKey() == "operon") {
            auto msg = ErrFormat("The operon feature at \"%s\" lacks an /operon qualifier.", location_to_string_or_unknown(featLocation).c_str());
            ErrPostStr(SEV_REJECT, ERR_FEATURE_MissingOperonQual, msg);
            success = false;
        }
    }

    for (const auto& resident : residentList) {
        bool matched = false;
        for (const auto& operon : operonList) {
            if (resident->mOperon != operon->mOperon) {
                continue;
            }
            matched                    = true;
            sequence::ECompare compare = sequence::Compare(
                *resident->mLocation, *operon->mLocation, nullptr, sequence::fCompareOverlapping);
            if (compare != sequence::eContained && compare != sequence::eSame) {
                auto msg = ErrFormat("Feature \"%s\" at \"%s\" with /operon qualifier \"%s\" does not fall within the span of the operon feature at \"%s\".", resident->mFeatname.c_str(), resident->LocationStr().c_str(), resident->mOperon.c_str(), operon->LocationStr().c_str());
                ErrPostStr(SEV_REJECT, ERR_FEATURE_OperonLocationMisMatch, msg);
                success = false;
            }
        }
        if (! matched) {
            auto msg = ErrFormat("/operon qualifier \"%s\" on feature \"%s\" at \"%s\" has a value that does not match any of the /operon qualifiers on operon features.", resident->mOperon.c_str(), resident->mFeatname.c_str(), resident->LocationStr().c_str());
            ErrPostStr(SEV_REJECT, ERR_FEATURE_InvalidOperonQual, msg);
            success = false;
        }
    }
    for (auto& resident : residentList) {
        delete resident;
    }
    for (auto& operon : operonList) {
        delete operon;
    }
    return success;
}

/**********************************************************/
static void fta_remove_dup_quals(FeatBlkPtr fbp)
{
    if (! fbp || fbp->quals.empty())
        return;

    for (TQualVector::iterator cur = fbp->quals.begin(); cur != fbp->quals.end(); ++cur) {
        const char* cur_qual = (*cur)->IsSetQual() ? (*cur)->GetQual().c_str() : nullptr;
        const char* cur_val  = (*cur)->IsSetVal() ? (*cur)->GetVal().c_str() : nullptr;

        TQualVector::iterator next = cur;
        for (++next; next != fbp->quals.end();) {
            const char* next_qual = (*next)->IsSetQual() ? (*next)->GetQual().c_str() : nullptr;
            const char* next_val  = (*next)->IsSetVal() ? (*next)->GetVal().c_str() : nullptr;

            if (! fta_strings_same(cur_qual, next_qual) || ! fta_strings_same(cur_val, next_val)) {
                ++next;
                continue;
            }

            string _loc = fbp->location_or("???");
            if (_loc.size() > 20) {
                _loc.resize(20);
                _loc += "...";
            }

            auto msg = ErrFormat("Duplicated qualifier \"%s\" in feature \"%s\" at location \"%s\" removed.", cur_qual ? cur_qual : "???", fbp->key_or("???"), _loc.c_str());
            ErrPostStr(SEV_ERROR, ERR_QUALIFIER_DuplicateRemoved, msg);

            next = fbp->quals.erase(next);
        }
    }
}

/**********************************************************/
static void CollectGapFeats(const DataBlk& entry, DataBlkPtr dbp, ParserPtr pp, Int2 type)
{
    IndexblkPtr ibp;
    GapFeatsPtr gfp = nullptr;
    DataBlkPtr  tdbp;
    FeatBlkPtr  fbp;

    CLinkage_evidence::TLinkage_evidence asn_linkage_evidence;
    list<string>                         linkage_evidence_names;

    StrNum*     snp;
    char*       p;
    char*       q;
    const char* gap_type;
    bool        finished_gap;
    ErrSev      sev;
    Int4        estimated_length;
    Int4        is_htg;
    Int4        from;
    Int4        to;
    Int4        prev_gap; /* 0 - initial, 1 - "gap",
                                           2 - "assembly_gap" */
    Int4        curr_gap; /* 0 - initial, 1 - "gap",
                                           2 - "assembly_gap" */
    CSeq_gap::TType asn_gap_type;

    ibp = pp->entrylist[pp->curindx];

    if (ibp->keywords.empty()) {
        if (pp->format == Parser::EFormat::GenBank)
            GetSequenceOfKeywords(entry, ParFlat_KEYWORDS, ParFlat_COL_DATA, ibp->keywords);
        else if (pp->format == Parser::EFormat::EMBL)
            GetSequenceOfKeywords(entry, ParFlat_KW, ParFlat_COL_DATA_EMBL, ibp->keywords);
        else if (pp->format == Parser::EFormat::XML)
            XMLGetKeywords(entry.mOffset, ibp->xip, ibp->keywords);
    }

    is_htg = -1;
    for (const string& key : ibp->keywords) {
        if (is_htg >= 0 && is_htg <= 2)
            break;
        if (key == "HTG")
            is_htg = 3;
        else if (key == "HTGS_PHASE0")
            is_htg = 0;
        else if (key == "HTGS_PHASE1")
            is_htg = 1;
        else if (key == "HTGS_PHASE2")
            is_htg = 2;
        else if (key == "HTGS_PHASE3")
            is_htg = 3;
    }

    // prev_gap     = 0;
    curr_gap     = 0;
    finished_gap = false;
    for (ibp->gaps = nullptr; dbp; dbp = dbp->mpNext) {
        if (ibp->drop)
            break;
        if (dbp->mType != type)
            continue;

        linkage_evidence_names.clear();
        asn_linkage_evidence.clear();

        for (tdbp = dbp->GetSubData(); tdbp; tdbp = tdbp->mpNext) {
            if (ibp->drop)
                break;
            fbp = tdbp->GetFeatData();
            if (! fbp || fbp->key.empty())
                continue;
            if (fbp->key == "gap") {
                prev_gap = curr_gap;
                curr_gap = 1;
            } else if (fbp->key == "assembly_gap") {
                prev_gap = curr_gap;
                curr_gap = 2;
            } else
                continue;

            from     = 0;
            to       = 0;
            gap_type = nullptr;
            linkage_evidence_names.clear();
            asn_gap_type = -1;
            asn_linkage_evidence.clear();
            estimated_length = -1;

            for (const auto& cur : fbp->quals) {
                if (! cur->IsSetQual() || ! cur->IsSetVal())
                    continue;

                const string& cur_qual = cur->GetQual();
                const string& cur_val  = cur->GetVal();

                if (cur_qual.empty() || cur_val.empty())
                    continue;

                if (cur_qual == "estimated_length") {
                    if (cur_val == "unknown")
                        estimated_length = -100;
                    else {
                        const char* cp = cur_val.c_str();
                        for (; *cp >= '0' && *cp <= '9';)
                            ++cp;
                        if (*cp == '\0')
                            estimated_length = atoi(cur_val.c_str());
                    }
                } else if (cur_qual == "gap_type")
                    gap_type = cur_val.c_str();
                else if (cur_qual == "linkage_evidence") {
                    linkage_evidence_names.push_back(cur_val);
                }
            }

            if (fbp->location_isset()) {
                p = fbp->location;
                if (*p == '<')
                    p++;
                for (q = p; *p >= '0' && *p <= '9';)
                    p++;
                if (*p == '\0') {
                    from = atoi(q);
                    to   = from;
                } else if (*p == '.') {
                    *p   = '\0';
                    from = atoi(q);
                    *p++ = '.';
                    if (*fbp->location == '<' && from != 1)
                        from = 0;
                    else if (*p == '.') {
                        if (*++p == '>')
                            p++;
                        for (q = p; *p >= '0' && *p <= '9';)
                            p++;
                        if (*p == '\0')
                            to = atoi(q);
                        if (*(q - 1) == '>' && to != (int)ibp->bases)
                            to = 0;
                    }
                }
            }

            if (from == 0 || to == 0 || from > to) {
                if (curr_gap == 1) {
                    auto msg = ErrFormat("Invalid gap feature location : \"%s\" : all gap features must have a simple X..Y location on the plus strand.", fbp->location_or("unknown"));
                    ErrPostStr(SEV_REJECT, ERR_FEATURE_InvalidGapLocation, msg);
                } else {
                    auto msg = ErrFormat("Invalid assembly_gap location : \"%s\".", fbp->location_or("unknown"));
                    ErrPostStr(SEV_REJECT, ERR_FEATURE_InvalidAssemblyGapLocation, msg);
                }
                ibp->drop = true;
                break;
            }

            if (curr_gap == 2) /* "assembly_gap" feature */
            {
                if (gap_type && is_htg > -1 &&
                    ! StringEqu(gap_type, "within scaffold") &&
                    ! StringEqu(gap_type, "repeat within scaffold")) {
                    auto msg = ErrFormat("assembly_gap has /gap_type of \"%s\", but clone-based HTG records are only expected to have \"within scaffold\" or \"repeat within scaffold\" gaps. assembly_gap feature located at \"%d..%d\".", gap_type, from, to);
                    ErrPostStr(SEV_ERROR, ERR_QUALIFIER_UnexpectedGapTypeForHTG, msg);
                }

                if (is_htg == 0 || is_htg == 1) {
                    for (const string& evidence : linkage_evidence_names) {
                        if (evidence != LinkageEvidenceValues[CLinkage_evidence_Base::eType_unspecified].str) {
                            auto msg = ErrFormat("assembly gap has /linkage_evidence of \"%s\", but unoriented and unordered Phase0/Phase1 HTG records are expected to have \"unspecified\" evidence. assembly_gap feature located at \"%d..%d\".", evidence.c_str(), from, to);
                            ErrPostStr(SEV_ERROR, ERR_QUALIFIER_LinkageShouldBeUnspecified, msg);
                        }
                    }
                } else if (is_htg == 2 || is_htg == 3) {
                    for (const string& evidence : linkage_evidence_names) {
                        if (evidence != LinkageEvidenceValues[CLinkage_evidence_Base::eType_unspecified].str)
                            continue;
                        auto msg = ErrFormat("assembly gap has /linkage_evidence of \"unspecified\", but ordered and oriented HTG records are expected to have some level of linkage for their gaps. assembly_gap feature located at \"%d..%d\".", from, to);
                        ErrPostStr(SEV_ERROR, ERR_QUALIFIER_LinkageShouldNotBeUnspecified, msg);
                    }
                }

                if (is_htg == 3 && ! finished_gap) {
                    auto msg = ErrFormat("Finished Phase-3 HTG records are not expected to have any gaps. First assembly_gap feature encountered at \"%d..%d\".", from, to);
                    ErrPostStr(SEV_ERROR, ERR_FEATURE_FinishedHTGHasAssemblyGap, msg);
                    finished_gap = true;
                }

                if (! gap_type) {
                    auto msg = ErrFormat("assembly_gap feature at \"%d..%d\" lacks the required /gap_type qualifier.", from, to);
                    ErrPostStr(SEV_REJECT, ERR_QUALIFIER_MissingGapType, msg);
                    ibp->drop = true;
                    break;
                }

                for (snp = GapTypeValues; snp->str; snp++)
                    if (StringEqu(snp->str, gap_type))
                        break;
                if (! snp->str) {
                    auto msg = ErrFormat("assembly_gap feature at \"%d..%d\" has an invalid gap type : \"%s\".", from, to, gap_type);
                    ErrPostStr(SEV_REJECT, ERR_QUALIFIER_InvalidGapType, msg);
                    ibp->drop = true;
                    break;
                }
                asn_gap_type = snp->num;

                if (linkage_evidence_names.empty() &&
                    (StringEqu(gap_type, "within scaffold") ||
                     StringEqu(gap_type, "repeat within scaffold"))) {
                    auto msg = ErrFormat("assembly_gap feature at \"%d..%d\" with gap type \"%s\" lacks a /linkage_evidence qualifier.", from, to, gap_type);
                    ErrPostStr(SEV_REJECT, ERR_QUALIFIER_MissingLinkageEvidence, msg);
                    ibp->drop = true;
                    break;
                }
                if (! linkage_evidence_names.empty()) {
                    if (! StringEqu(gap_type, "unknown") &&
                        ! StringEqu(gap_type, "within scaffold") &&
                        ! StringEqu(gap_type, "repeat within scaffold")) {
                        auto msg = ErrFormat("The /linkage_evidence qualifier is not legal for the assembly_gap feature at \"%d..%d\" with /gap_type \"%s\".", from, to, gap_type);
                        ErrPostStr(SEV_REJECT, ERR_QUALIFIER_InvalidGapTypeForLinkageEvidence, msg);
                        ibp->drop = true;
                        break;
                    }

                    for (const string& evidence : linkage_evidence_names) {
                        for (snp = LinkageEvidenceValues; snp->str; snp++)
                            if (evidence == snp->str)
                                break;
                        if (! snp->str) {
                            auto msg = ErrFormat("assembly_gap feature at \"%d..%d\" has an invalid linkage evidence : \"%s\".", from, to, evidence.c_str());
                            ErrPostStr(SEV_REJECT, ERR_QUALIFIER_InvalidLinkageEvidence, msg);
                            ibp->drop = true;
                            break;
                        }

                        CRef<CLinkage_evidence> new_evidence(new CLinkage_evidence);
                        new_evidence->SetType(snp->num);
                        asn_linkage_evidence.push_back(new_evidence);
                    }
                }
            }

            if (prev_gap + curr_gap == 3) {
                string msg;
                if (curr_gap == 1)
                    msg = ErrFormat("Legacy gap feature at \"%d..%d\" co-exists with a new AGP 2.0 assembly_gap feature at \"%d..%d\".", from, to, gfp->from, gfp->to);
                else
                    msg = ErrFormat("Legacy gap feature at \"%d..%d\" co-exists with a new AGP 2.0 assembly_gap feature at \"%d..%d\".", gfp->from, gfp->to, from, to);
                ErrPostStr(SEV_REJECT, ERR_FEATURE_AssemblyGapAndLegacyGap, msg);
                ibp->drop = true;
                break;
            }

            if (estimated_length == -1) /* missing qual */
            {
                auto msg = ErrFormat("The gap feature at \"%d..%d\" lacks the required /estimated_length qualifier.", from, to);
                ErrPostStr(SEV_REJECT, ERR_FEATURE_RequiredQualifierMissing, msg);
                ibp->drop = true;
            } else if (estimated_length == 0) {
                auto msg = ErrFormat("Gap feature at \"%d..%d\" has an illegal /estimated_length qualifier : \"%s\" : should be \"unknown\" or an integer.", from, to, "");
                // from, to, gbqp->val); // at this point gbqp is definitely = NULL
                ErrPostStr(SEV_REJECT, ERR_FEATURE_IllegalEstimatedLength, msg);
                ibp->drop = true;
            } else if (estimated_length == -100) {
                if (is_htg >= 0 && to - from != 99) {
                    auto msg = ErrFormat("Gap feature at \"%d..%d\" has /estimated_length \"unknown\" but the gap size is not 100 bases.", from, to);
                    ErrPostStr(SEV_ERROR, ERR_FEATURE_UnknownGapNot100, msg);
                }
            } else if (estimated_length != to - from + 1) {
                if (pp->source == Parser::ESource::EMBL || pp->source == Parser::ESource::DDBJ)
                    sev = SEV_ERROR;
                else {
                    sev       = SEV_REJECT;
                    ibp->drop = true;
                }

                auto msg = ErrFormat("Gap feature at \"%d..%d\" has a size that does not match the /estimated_length : %d.", from, to, estimated_length);
                ErrPostStr(sev, ERR_FEATURE_GapSizeEstLengthMissMatch, msg);
            }

            for (gfp = ibp->gaps; gfp; ++gfp) {
                if ((gfp->from >= from && gfp->from <= to) ||
                    (gfp->to >= from && gfp->to <= to) ||
                    (gfp->from <= from && gfp->to >= to)) {
                    auto msg = ErrFormat("Gap features at \"%d..%d\" and \"%d..%d\" overlap.", from, to, gfp->from, gfp->to);
                    ErrPostStr(SEV_REJECT, ERR_FEATURE_OverlappingGaps, msg);
                    ibp->drop = true;
                } else if (to + 1 == gfp->from || from - 1 == gfp->to) {
                    if (pp->source == Parser::ESource::EMBL)
                        sev = SEV_ERROR;
                    else {
                        sev       = SEV_REJECT;
                        ibp->drop = true;
                    }

                    auto msg = ErrFormat("Gap features at \"%d..%d\" and \"%d..%d\" are contiguous, and should probably be represented by a single gap that spans both.", from, to, gfp->from, gfp->to);
                    ErrPostStr(sev, ERR_FEATURE_ContiguousGaps, msg);
                }
            }
            if (ibp->drop)
                break;

            gfp                   = new GapFeats;
            gfp->from             = from;
            gfp->to               = to;
            gfp->estimated_length = estimated_length;
            if (curr_gap == 2) /* /assembly_gap feature */
                gfp->assembly_gap = true;
            if (gap_type) {
                gfp->gap_type     = gap_type;
                gfp->asn_gap_type = asn_gap_type;
            }
            if (! asn_linkage_evidence.empty()) {
                gfp->asn_linkage_evidence.swap(asn_linkage_evidence);
                asn_linkage_evidence.clear();
            }
            gfp->next = nullptr;

            if (! ibp->gaps) {
                ibp->gaps = gfp;
                continue;
            }

            if (ibp->gaps->from > from) {
                gfp->next = ibp->gaps.node;
                ibp->gaps = gfp;
                continue;
            }

            if (! ibp->gaps->next) {
                ibp->gaps->next = gfp.node;
                continue;
            }

            for (GapFeatsPtr tgfp = ibp->gaps; tgfp; ++tgfp) {
                if (tgfp->next && tgfp->next->from < from)
                    continue;
                gfp->next  = tgfp->next;
                tgfp->next = gfp.node;
                break;
            }
        }
        if (ibp->drop) {
            linkage_evidence_names.clear();
            asn_linkage_evidence.clear();
        }
    }

    if (! ibp->gaps)
        return;

    if (ibp->drop) {
        GapFeatsFree(ibp->gaps);
        ibp->gaps = nullptr;
    }
}

/**********************************************************/
static void XMLGetQuals(char* entry, XmlIndexPtr xip, TQualVector& quals)
{
    XmlIndexPtr xipqual;

    if (! entry || ! xip)
        return;

    for (; xip; xip = xip->next) {
        if (! xip->subtags)
            continue;

        CRef<CGb_qual> qual(new CGb_qual);
        for (xipqual = xip->subtags; xipqual; xipqual = xipqual->next) {
            if (xipqual->tag == INSDQUALIFIER_NAME)
                qual->SetQual(*XMLGetTagValue(entry, xipqual));
            else if (xipqual->tag == INSDQUALIFIER_VALUE)
                qual->SetVal(*XMLGetTagValue(entry, xipqual));
        }

        if (qual->GetQual() == "replace" && ! qual->IsSetVal()) {
            qual->SetVal("");
        }

        if (qual->IsSetQual() && ! qual->GetQual().empty())
            quals.push_back(qual);
    }
}

/**********************************************************/
static DataBlkPtr XMLLoadFeatBlk(char* entry, XmlIndexPtr xip)
{
    XmlIndexPtr xipfeat;
    DataBlkPtr  headdbp;
    DataBlkPtr  dbp;
    DataBlkPtr  ret;
    FeatBlkPtr  fbp;

    if (! entry || ! xip)
        return nullptr;

    for (; xip; xip = xip->next)
        if (xip->tag == INSDSEQ_FEATURE_TABLE)
            break;

    if (! xip || ! xip->subtags)
        return nullptr;

    headdbp = nullptr;
    for (xip = xip->subtags; xip; xip = xip->next) {
        if (! xip->subtags)
            continue;
        fbp          = new FeatBlk;
        fbp->spindex = -1;
        for (xipfeat = xip->subtags; xipfeat; xipfeat = xipfeat->next) {
            if (xipfeat->tag == INSDFEATURE_KEY)
                fbp->key = *XMLGetTagValue(entry, xipfeat);
            else if (xipfeat->tag == INSDFEATURE_LOCATION)
                fbp->location_assign(*XMLGetTagValue(entry, xipfeat));
            else if (xipfeat->tag == INSDFEATURE_QUALS)
                XMLGetQuals(entry, xipfeat->subtags, fbp->quals);
        }
        if (! headdbp) {
            headdbp = new DataBlk;
            dbp     = headdbp;
        } else {
            dbp->mpNext = new DataBlk;
            dbp         = dbp->mpNext;
        }
        dbp->SetFeatData(fbp);
    }
    ret         = new DataBlk;
    ret->mType  = XML_FEATURES;
    ret->SetSubData(headdbp);
    ret->mpNext = nullptr;
    return (ret);
}

/**********************************************************
 *
 *   static FeatBlkPtr MergeNoteQual(fbp):
 *
 *      Only one note on every key feature block,
 *   not complete.
 *
 *                                              5-28-93
 *
 **********************************************************/
static FeatBlkPtr MergeNoteQual(FeatBlkPtr fbp)
{
    char* p;
    char* q;

    size_t size = 0;

    for (auto& cur : fbp->quals) {
        if (! cur->IsSetQual() || ! cur->IsSetVal())
            continue;

        const string& cur_qual = cur->GetQual();
        const string& cur_val  = cur->GetVal();

        if (cur_qual != "note" || cur_val.empty())
            continue;

        size += 2;
        vector<Char> buf(cur_val.size() + 1);

        const char* cp = cur_val.c_str();
        for (q = &buf[0]; *cp != '\0'; ++cp) {
            *q++ = *cp;
            if (*cp == ';' && (cp[1] == ' ' || cp[1] == ';')) {
                for (++cp; *cp == ' ' || *cp == ';';)
                    ++cp;
                if (*cp != '\0')
                    *q++ = ' ';
                --cp;
            }
        }

        *q = '\0';
        cur->SetVal(&buf[0]);

        size += cur->GetVal().size();
        for (cp = cur->GetVal().c_str(); *cp != '\0'; ++cp)
            if (*cp == '~')
                ++size;
    }

    if (size == 0)
        return (fbp);

    char* note = StringNew(size - 1);
    p          = note;

    for (TQualVector::iterator cur = fbp->quals.begin(); cur != fbp->quals.end();) {
        if (! (*cur)->IsSetQual() || ! (*cur)->IsSetVal()) {
            ++cur;
            continue;
        }

        const string& cur_qual = (*cur)->GetQual();
        const string& cur_val  = (*cur)->GetVal();

        if (cur_qual != "note") {
            ++cur;
            continue;
        }

        if (! cur_val.empty()) {
            /* sometime we get note qual w/o value
             */
            if (p > note) {
                *p++ = ';';
                *p++ = '~';
            }

            for (const char* cq = cur_val.c_str(); *cq != '\0'; *p++ = *cq++)
                if (*cq == '~')
                    *p++ = '~';
        }

        cur = fbp->quals.erase(cur);
    }
    *p = '\0';

    CRef<CGb_qual> qual_new(new CGb_qual);
    qual_new->SetQual("note");
    qual_new->SetVal(note);
    MemFree(note);

    fbp->quals.push_back(qual_new);

    return (fbp);
}

/**********************************************************/
static bool CheckLegalQual(const Char* val, Char ch, string* qual)
{
    string qual_name;
    for (; *val && *val != ch && (isalpha(*val) || *val == '_'); ++val)
        qual_name += *val;

    CSeqFeatData::EQualifier type = CSeqFeatData::GetQualifierType(qual_name);
    if (type == CSeqFeatData::eQual_bad)
        return false;

    if (qual)
        *qual = qual_name;

    return true;
}

/**********************************************************/
static void fta_convert_to_lower_case(char* str)
{
    char* p;

    if (! str || *str == '\0')
        return;

    for (p = str; *p != '\0'; p++)
        if (*p >= 'A' && *p <= 'Z')
            *p |= 040;
}

/**********************************************************/
static void fta_process_con_slice(vector<char>& val_buf)
{
    size_t i = 1;
    char*  p = &val_buf[0];

    // look for commas not followed by blank or end-of-string
    for (; *p != '\0'; p++)
        if (*p == ',' && p[1] != ' ' && p[1] != '\0')
            i++;

    // if there are some ...
    if (i > 1) {
        vector<char> buf(i + val_buf.size());
        char*        q = &buf[0];
        // ... then insert a blank right after the comma
        for (p = &val_buf[0]; *p != '\0'; p++) {
            *q++ = *p;
            if (*p == ',' && p[1] != ' ' && p[1] != '\0')
                *q++ = ' ';
        }
        *q = '\0';
        val_buf.swap(buf);
    }
}


void xSplitLines(
    const string&   str,
    vector<string>& lines)
{
    NStr::Split(str, "\n", lines, 0);
}

/**********************************************************
 *
 *   static void ParseQualifiers(fbp, bptr, eptr,
 *                               format):
 *
 *      Parsing qualifier and put into link list fbp->qual.
 *      Some qualifiers may not have value.
 *      genbank qualifier format:  /qualifier=value
 *      embl qualifier format:     /qualifier= value
 *
 *                                              10-12-93
 *
 **********************************************************/
static void ParseQualifiers(
    FeatBlkPtr      fbp,
    const char*     bptr,
    const char*     eptr,
    Parser::EFormat format)
{
    string bstr(bptr, eptr);
    NStr::TruncateSpacesInPlace(bstr);
    // cerr << "bstr:\n" << bstr.c_str() << "\n\n";
    vector<string> qualLines;
    xSplitLines(bstr, qualLines);

    string      qualKey, qualVal;
    string      featKey(fbp->key);
    string      featLocation(fbp->location_get());
    CQualParser qualParser(format, featKey, featLocation, qualLines);
    while (! qualParser.Done()) {
        if (qualParser.GetNextQualifier(qualKey, qualVal)) {
            // cerr << "Key:   " << qualKey.c_str() << "\n";
            // cerr << "Val:   " << qualVal.c_str() << "\n";
            CRef<CGb_qual> pQual(new CGb_qual);
            pQual->SetQual(qualKey);
            pQual->SetVal(qualVal);
            fbp->quals.push_back(pQual);
        }
    }
}


/**********************************************************/
static void fta_check_satellite(char* str, bool* drop)
{
    char* p;
    Int2  i;

    if (! str || *str == '\0')
        return;

    p = StringChr(str, ':');
    if (p)
        *p = '\0';

    i = MatchArrayString(SatelliteValues, str);
    if (p)
        *p = ':';
    if (i < 0) {
        auto msg = ErrFormat("/satellite qualifier \"%s\" does not begin with a valid satellite type.", str);
        ErrPostStr(SEV_REJECT, ERR_FEATURE_InvalidSatelliteType, msg);
        *drop = true;
    } else if (p && p[1] == '\0') {
        auto msg = ErrFormat("/satellite qualifier \"%s\" does not include a class or identifier after the satellite type.", str);
        ErrPostStr(SEV_REJECT, ERR_FEATURE_NoSatelliteClassOrIdentifier, msg);
        *drop = true;
    }
}

/**********************************************************
 *
 *   int ParseFeatureBlock(ibp, deb, dbp, source, format):
 *
 *      Parsing each feature sub-block, dbp, to
 *   FeatBlkPtr, fbp.
 *      Put warning message if bad qualifier's value or
 *   unknown feature key found.
 *      fdbp->drop = true, if found unknown feature key, or
 *   do not go through 2nd time of qualifiers sematic
 *   check (i.e. drop bad qualifier if the value if illegal
 *   format in the 1st time)
 *
 *                                              11-22-93
 *
 *      The location begins at column 22, and qualifier
 *   begin on subsequent lines at column 22, they may
 *   extend from column 22-80.
 *      Qualifiers take the form of a slash, "/", followed
 *   by the qualifier name and, if applicable, an equal
 *   sign, "=", and a value (i.e. some qualifiers only
 *   have name w/o value, s.t. /pseudo).
 *
 *                                              5-4-93
 *
 **********************************************************/
int ParseFeatureBlock(IndexblkPtr ibp, bool deb, DataBlkPtr dbp, Parser::ESource source, Parser::EFormat format)
{
    char* bptr;
    char* eptr;
    char* ptr1;
    char* ptr2;
    char* p;
    char* q;
    string loc;

    FeatBlkPtr fbp;
    Int4       num;
    size_t     i;
    int        retval = GB_FEAT_ERR_NONE;
    int        ret;

    if (ibp->is_mga)
        loc = "1.." + to_string(ibp->bases);
    for (num = 0; dbp; dbp = dbp->mpNext, num++) {
        fbp          = new FeatBlk;
        fbp->spindex = -1;
        fbp->num     = num;
        dbp->SetFeatData(fbp);

        bptr = dbp->mOffset;
        eptr = bptr + dbp->len;

        for (p = bptr; *p != '\n';)
            p++;
        *p = '\0';
        FtaInstallPrefix(PREFIX_FEATURE, "Parsing FT line: ", bptr);
        *p   = '\n';
        ptr1 = bptr + ParFlat_COL_FEATKEY;
        if (*ptr1 == ' ') {
            ErrPostStr(SEV_WARNING, ERR_FEATURE_FeatureKeyReplaced, "Empty featkey");
        }
        for (ptr1 = bptr; *ptr1 == ' ';)
            ptr1++;

        for (ptr2 = ptr1; *ptr2 != ' ' && *ptr2 != '\n';)
            ptr2++;

        if (StringEquN(ptr1, "- ", 2)) {
            ErrPostStr(SEV_WARNING, ERR_FEATURE_FeatureKeyReplaced, "Featkey '-' is replaced by 'misc_feature'");
            fbp->key = "misc_feature";
        } else
            fbp->key = string_view(ptr1, ptr2 - ptr1);

        for (ptr1 = ptr2; *ptr1 == ' ';)
            ptr1++;
        if (*ptr1 == '\n') {
            if (ibp->is_mga == false) {
                ErrPostStr(SEV_WARNING, ERR_FEATURE_LocationParsing, "Location missing");
                dbp->mDrop = true;
                retval     = GB_FEAT_ERR_DROP;
                continue;
            }
        } else {
            i = ptr1 - bptr;
            if (i < ParFlat_COL_FEATDAT)
                ErrPostStr(SEV_WARNING, ERR_FEATURE_LocationParsing, "Location data is shifted to the left");
            else if (i > ParFlat_COL_FEATDAT)
                ErrPostStr(SEV_WARNING, ERR_FEATURE_LocationParsing, "Location data is shifted to the right");
        }

        for (ptr2 = ptr1; *ptr2 != '/' && ptr2 < eptr;)
            ptr2++;
        fbp->location_assign(string_view(ptr1, ptr2 - ptr1));
        if (ibp->is_prot)
            fta_strip_aa(fbp->location);
        for (p = fbp->location, q = p; *p != '\0'; p++)
            if (*p != ' ' && *p != '\n')
                *q++ = *p;
        *q = '\0';

        if (fbp->location[0] == '\0' && ibp->is_mga) {
            MemFree(fbp->location);
            fbp->location_assign(loc);
        }

        FtaInstallPrefix(PREFIX_FEATURE, fbp->key.c_str(), fbp->location_get());
        if (fbp->key == "allele" || fbp->key == "mutation") {
            auto msg = ErrFormat("Obsolete feature \"%s\" found. Replaced with \"variation\".", fbp->key.c_str());
            ErrPostStr(SEV_ERROR, ERR_FEATURE_ObsoleteFeature, msg);
            fbp->key = "variation";
        }

        CSeqFeatData::ESubtype subtype = CSeqFeatData::SubtypeNameToValue(fbp->key);

        if (subtype == CSeqFeatData::eSubtype_bad && ! deb) {
            ErrPostEx(SEV_ERROR, ERR_FEATURE_UnknownFeatKey, fbp->key.c_str(), "Feature dropped");
            dbp->mDrop = true;
            retval     = GB_FEAT_ERR_DROP;
            continue;
        }

        if (*ptr2 == '/') /* qualifier start in first "/" */
        {
            ParseQualifiers(fbp, ptr2, eptr, format);

            if (fbp->key != "assembly_gap") {
                for (const auto& cur : fbp->quals) {
                    const string& cur_qual = cur->GetQual();
                    if (cur_qual == "gap_type" ||
                        cur_qual == "assembly_evidence") {
                        auto msg = ErrFormat("Qualifier /%s is invalid for the feature \"%s\" at \"%s\".", cur_qual.c_str(), fbp->key.c_str(), fbp->location_or("Unknown"));
                        ErrPostStr(SEV_REJECT, ERR_FEATURE_InvalidQualifier, msg);
                        ibp->drop = true;
                    }
                }
            }

            if (fbp->key != "source") {
                for (const auto& cur : fbp->quals) {
                    const string& cur_qual = cur->GetQual();
                    if (cur_qual == "submitter_seqid") {
                        auto msg = ErrFormat("Qualifier /%s is invalid for the feature \"%s\" at \"%s\".", cur_qual.c_str(), fbp->key.c_str(), fbp->location_or("Unknown"));
                        ErrPostStr(SEV_REJECT, ERR_FEATURE_InvalidQualifier, msg);
                        ibp->drop = true;
                    }
                }
            }

            fbp = MergeNoteQual(fbp); /* allow more than one
                                           notes w/i a key */

            if (subtype == CSeqFeatData::eSubtype_bad) {
                ErrPostStr(SEV_ERROR, ERR_FEATURE_UnknownFeatKey, fbp->key);
                ret = GB_FEAT_ERR_REPAIRABLE;
            } else {
                /* last argument is perform_corrections if debug
                 * mode is FALSE
                 */
                ret = XGBFeatKeyQualValid(subtype, fbp->quals, true, (source == Parser::ESource::Flybase ? false : ! deb));
            }
            if (ret > retval)
                retval = ret;

            if (ret > GB_FEAT_ERR_REPAIRABLE && fbp->key != "ncRNA")
                dbp->mDrop = true;
        } else if (subtype == CSeqFeatData::eSubtype_bad && ! CSeqFeatData::GetMandatoryQualifiers(subtype).empty()) {
            if (fbp->key != "mobile_element") {
                auto   qual_idx = *CSeqFeatData::GetMandatoryQualifiers(subtype).begin();
                string str      = CSeqFeatData::GetQualifierAsString(qual_idx);
                if ((fbp->key != "old_sequence" && fbp->key != "conflict") ||
                    (str != "citation")) {
                    auto msg = ErrFormat("lacks required /%s qualifier : feature has been dropped.", str.c_str());
                    ErrPostStr(SEV_ERROR, ERR_FEATURE_RequiredQualifierMissing, msg);
                    if (! deb) {
                        dbp->mDrop = true;
                        retval     = GB_FEAT_ERR_DROP;
                    }
                }
            }
        } else if (fbp->key == "misc_feature" && fbp->quals.empty()) {
            if (! deb) {
                dbp->mDrop = true;
                retval     = GB_FEAT_ERR_DROP;
                ErrPostStr(SEV_WARNING, ERR_FEATURE_Dropped, "Empty 'misc_feature' dropped");
            } else
                retval = GB_FEAT_ERR_REPAIRABLE;
        }

        for (auto& cur : fbp->quals) {
            if (! cur->IsSetQual() || ! cur->IsSetVal())
                continue;

            const string& qual_str = cur->GetQual();
            string        val_str  = cur->GetVal();

            ShrinkSpaces(val_str);
            vector<Char> val_buf(val_str.begin(), val_str.end());
            val_buf.push_back(0);

            p = &val_buf[0];
            if (*p == '\0' && qual_str != "replace") {
                cur->ResetVal();
                val_buf[0] = 0;
            } else {
                if (qual_str == "replace")
                    fta_convert_to_lower_case(p);
                cur->SetVal(p);
            }

            if (qual_str == "satellite")
                fta_check_satellite(&val_buf[0], &ibp->drop);
        }
    } /* for, each sub-block, or each feature key */
    FtaDeletePrefix(PREFIX_FEATURE);
    return (retval);
}

/**********************************************************/
static void XMLCheckQualifiers(FeatBlkPtr fbp)
{
    const char** b;
    char*        p;

    if (! fbp || fbp->quals.empty())
        return;

    for (TQualVector::iterator cur = fbp->quals.begin(); cur != fbp->quals.end();) {
        const string& qual_str = (*cur)->GetQual();

        if ((*cur)->IsSetVal()) {
            const string& val_str = (*cur)->GetVal();
            vector<Char>  val_buf(val_str.begin(), val_str.end());
            val_buf.push_back(0);

            if (qual_str == "translation") {
                DelCharBtwData(&val_buf[0]);
            } else if (qual_str == "rpt_unit") {
                fta_convert_to_lower_case(&val_buf[0]);
            } else if (qual_str == "cons_splice") {
                fta_process_con_slice(val_buf);
            } else if (qual_str == "note") {
                for (p = &val_buf[0];;) {
                    p = StringChr(p, '/');
                    if (! p)
                        break;
                    p++;
                    if (! CheckLegalQual(p, ' ', nullptr))
                        continue;

                    string _loc(val_buf.begin(), val_buf.end());
                    if (_loc.size() > 30) {
                        _loc.resize(30);
                        _loc += " ...";
                    }
                    auto msg = ErrFormat("/note qualifier value appears to contain other qualifiers : [%s].", _loc.c_str());
                    ErrPostStr(SEV_WARNING, ERR_QUALIFIER_EmbeddedQual, msg);
                }
            }

            for (p = &val_buf[0]; *p == '\"' || *p == ' ' || *p == '\t';)
                p++;

            if (*p == '\0') {
                if (qual_str == "replace") {
                    (*cur)->SetVal("");
                } else
                    (*cur)->ResetVal();
            } else
                (*cur)->SetVal(&val_buf[0]);
        }

        for (b = EmptyQuals; *b; b++)
            if (qual_str == *b)
                break;

        if (! *b) {
            if (! (*cur)->IsSetVal()) {
                if (qual_str == "old_locus_tag") {
                    auto msg = ErrFormat("Feature \"%s\" at \"%s\" has an /old_locus_tag qualifier with no value. Qualifier has been dropped.", fbp->key_or("Unknown"), fbp->location_or("Empty"));
                    ErrPostStr(SEV_ERROR, ERR_FEATURE_EmptyOldLocusTag, msg);
                } else {
                    auto msg = ErrFormat("Qualifier /%s ignored because it lacks a data value. Feature \"%s\", location \"%s\".", qual_str.c_str(), fbp->key_or("Unknown"), fbp->location_or("Empty"));
                    ErrPostStr(SEV_WARNING, ERR_QUALIFIER_EmptyQual, msg);
                }
                cur = fbp->quals.erase(cur);
                continue;
            }
        } else if ((*cur)->IsSetVal()) {
            auto msg = ErrFormat("Qualifier /%s should not have data value. Qualifier value has been ignored. Feature \"%s\", location \"%s\".", qual_str.c_str(), fbp->key_or("Unknown"), fbp->location_or("Empty"));
            ErrPostStr(SEV_WARNING, ERR_QUALIFIER_ShouldNotHaveValue, msg);
            (*cur)->ResetVal();
        }

        if ((*cur)->IsSetVal() && qual_str == "note") {
            string val = (*cur)->GetVal();
            std::replace(val.begin(), val.end(), '\"', '\'');
            (*cur)->SetVal(val);
        }

        ++cur;
    }
}

/**********************************************************/
static int XMLParseFeatureBlock(bool deb, DataBlkPtr dbp, Parser::ESource source)
{
    FeatBlkPtr fbp;
    char*      p;
    Int4       num;
    Int2       keyindx;
    int        retval = GB_FEAT_ERR_NONE;
    int        ret    = 0;

    for (num = 0; dbp; dbp = dbp->mpNext, num++) {
        if (! dbp->hasData())
            continue;
        fbp      = dbp->GetFeatData();
        fbp->num = num;
        FtaInstallPrefix(PREFIX_FEATURE, fbp->key.c_str(), fbp->location_get());

        if (fbp->key == "-") {
            ErrPostStr(SEV_WARNING, ERR_FEATURE_FeatureKeyReplaced, "Featkey '-' is replaced by 'misc_feature'");
            fbp->key = "misc_feature";
        }

        if (fbp->key == "allele" || fbp->key == "mutation") {
            auto msg = ErrFormat("Obsolete feature \"%s\" found. Replaced with \"variation\".", fbp->key.c_str());
            ErrPostStr(SEV_ERROR, ERR_FEATURE_ObsoleteFeature, msg);
            fbp->key = "variation";
        }

        CSeqFeatData::ESubtype subtype = CSeqFeatData::SubtypeNameToValue(fbp->key);

        /* bsv hack: exclude CONFLICT, REGION, SITE, UNSURE UniProt flatfile
         *           features from valid GenBank ones: for USPTO only
         * Needs better workaround
         */
        if (source == Parser::ESource::USPTO &&
            (subtype == CSeqFeatData::eSubtype_conflict ||
             subtype == CSeqFeatData::eSubtype_region ||
             subtype == CSeqFeatData::eSubtype_site ||
             subtype == CSeqFeatData::eSubtype_unsure))
            subtype = CSeqFeatData::eSubtype_bad;
        keyindx = -1;
        if (subtype == CSeqFeatData::eSubtype_bad && ! deb) {
            if (source == Parser::ESource::USPTO)
                keyindx = SpFeatKeyNameValid(fbp->key.c_str());
            if (keyindx < 0 && ! deb) {
                ErrPostEx(SEV_ERROR, ERR_FEATURE_UnknownFeatKey, fbp->key.c_str(), "Feature dropped");
                dbp->mDrop = true;
                retval     = GB_FEAT_ERR_DROP;
                continue;
            }
            fbp->spindex = keyindx;
        }

        if (! fbp->quals.empty()) {
            XMLCheckQualifiers(fbp);
            fbp = MergeNoteQual(fbp); /* allow more than one
                                           notes w/i a key */

            if (subtype == CSeqFeatData::eSubtype_bad) {
                if (keyindx < 0) {
                    ErrPostStr(SEV_ERROR, ERR_FEATURE_UnknownFeatKey, fbp->key);
                    ret = GB_FEAT_ERR_REPAIRABLE;
                }
            } else if (fbp->spindex < 0) {
                /* last argument is perform_corrections if debug
                 * mode is FALSE
                 */
                ret = XGBFeatKeyQualValid(subtype, fbp->quals, true, ((source == Parser::ESource::Flybase) ? false : ! deb));
            }
            if (ret > retval)
                retval = ret;

            if (ret > GB_FEAT_ERR_REPAIRABLE && fbp->key != "ncRNA")
                dbp->mDrop = true;
        } else if (subtype == CSeqFeatData::eSubtype_bad && ! CSeqFeatData::GetMandatoryQualifiers(subtype).empty()) {
            if (fbp->key != "mobile_element") {
                auto   qual_idx = *CSeqFeatData::GetMandatoryQualifiers(subtype).begin();
                string str      = CSeqFeatData::GetQualifierAsString(qual_idx);
                if ((fbp->key != "old_sequence" && fbp->key != "conflict") ||
                    (str != "citation")) {
                    auto msg = ErrFormat("lacks required /%s qualifier : feature has been dropped.", str.c_str());
                    ErrPostStr(SEV_ERROR, ERR_FEATURE_RequiredQualifierMissing, msg);
                    if (! deb) {
                        dbp->mDrop = true;
                        retval     = GB_FEAT_ERR_DROP;
                    }
                }
            }
        } else if (fbp->key == "misc_feature" && fbp->quals.empty()) {
            if (! deb) {
                dbp->mDrop = true;
                retval     = GB_FEAT_ERR_DROP;
                ErrPostStr(SEV_WARNING, ERR_FEATURE_Dropped, "Empty 'misc_feature' dropped");
            } else
                retval = GB_FEAT_ERR_REPAIRABLE;
        }

        for (auto& cur : fbp->quals) {
            if (! cur->IsSetQual() || ! cur->IsSetVal())
                continue;

            const string& qual_str = cur->GetQual();
            string        val_str  = cur->GetVal();

            ShrinkSpaces(val_str);
            vector<Char> val_buf(val_str.begin(), val_str.end());
            val_buf.push_back(0);

            p = &val_buf[0];
            if (*p == '\0' && qual_str != "replace") {
                cur->ResetVal();
                val_buf[0] = 0;
            } else {
                if (qual_str == "replace")
                    fta_convert_to_lower_case(p);
                cur->SetVal(p);
            }
        }
    } /* for, each sub-block, or each feature key */
    FtaDeletePrefix(PREFIX_FEATURE);
    return (retval);
}

/**********************************************************/
static bool fta_check_ncrna(const CSeq_feat& feat)
{
    int count = 0;

    bool stop = false;
    for (const auto& qual : feat.GetQual()) {
        if (! qual->IsSetQual() || qual->GetQual().empty() ||
            qual->GetQual() != "ncRNA_class")
            continue;

        count++;

        if (! qual->IsSetVal() || qual->GetVal().empty()) {
            string loc = location_to_string_or_unknown(feat.GetLocation());
            string msg = ErrFormat("Feature \"ncRNA\" at location \"%s\" has an empty /ncRNA_class qualifier.", loc.empty() ? "unknown" : loc.c_str());
            ErrPostStr(SEV_REJECT, ERR_FEATURE_ncRNA_class, msg);
            stop = true;
            break;
        }

        if (MatchArrayString(ncRNA_class_values, qual->GetVal().c_str()) < 0) {
            string loc = location_to_string_or_unknown(feat.GetLocation());
            string msg = ErrFormat("Feature \"ncRNA\" at location \"%s\" has an invalid /ncRNA_class qualifier: \"%s\".", loc.empty() ? "unknown" : loc.c_str(), qual->GetVal().c_str());
            ErrPostStr(SEV_REJECT, ERR_FEATURE_ncRNA_class, msg);
            stop = true;
            break;
        }
    }

    if (stop)
        return false;

    if (count == 1)
        return true;

    string loc = location_to_string_or_unknown(feat.GetLocation());
    string msg = ErrFormat("Feature \"ncRNA\" at location \"%s\" %s /ncRNA_class qualifier.", loc.empty() ? "unknown" : loc.c_str(), (count == 0) ? "lacks the mandatory" : "has more than one");
    ErrPostStr(SEV_REJECT, ERR_FEATURE_ncRNA_class, msg);

    return false;
}

/**********************************************************/
static void fta_check_artificial_location(CSeq_feat& feat, const string& key)
{
    for (auto qual = feat.SetQual().begin(); qual != feat.SetQual().end(); ++qual) {
        if (! (*qual)->IsSetQual() || (*qual)->GetQual() != "artificial_location")
            continue;

        if ((*qual)->IsSetVal()) {
            const Char* p_val = (*qual)->GetVal().c_str();
            for (; *p_val == '\"';)
                ++p_val;

            if (*p_val == '\0')
                (*qual)->ResetVal();
        }

        string val = (*qual)->IsSetVal() ? (*qual)->GetVal() : "";

        if (val == "heterogenous population sequenced" ||
            val == "low-quality sequence region") {
            feat.SetExcept(true);

            if (! feat.IsSetExcept_text())
                feat.SetExcept_text(val);
            else {
                string& except_text = feat.SetExcept_text();
                except_text += ", ";
                except_text += val;
            }
        } else {
            string loc_str = location_to_string_or_unknown(feat.GetLocation());
            string msg;
            if (val.empty())
                msg = ErrFormat("Encountered empty /artificial_location qualifier : Feature \"%s\" : Location \"%s\". Qualifier dropped.", key.empty() ? "unknown" : key.c_str(), loc_str.empty() ? "unknown" : loc_str.c_str());
            else
                msg = ErrFormat("Value \"%s\" is not legal for the /artificial_location qualifier : Feature \"%s\" : Location \"%s\". Qualifier dropped.", val.c_str(), key.empty() ? "unknown" : key.c_str(), loc_str.empty() ? "unknown" : loc_str.c_str());
            ErrPostStr(SEV_ERROR, ERR_QUALIFIER_InvalidArtificialLoc, msg);
        }

        feat.SetQual().erase(qual);
        break;
    }
}

/**********************************************************/
static bool fta_check_mobile_element(const CSeq_feat& feat)
{
    bool found = false;
    for (const auto& qual : feat.GetQual()) {
        if (qual->IsSetQual() && qual->GetQual() == "mobile_element_type" &&
            qual->IsSetVal() && ! qual->GetVal().empty()) {
            const Char* p_val = qual->GetVal().c_str();
            for (; *p_val == '\"';)
                ++p_val;

            if (*p_val != '\0') {
                found = true;
                break;
            }
        }
    }

    if (found)
        return true;

    auto loc_str = location_to_string_or_unknown(feat.GetLocation());
    auto msg = ErrFormat("Mandatory qualifier /mobile_element_type is absent or has no value : Feature \"mobile_element\" : Location \"%s\". Entry dropped.", loc_str.empty() ? "unknown" : loc_str.c_str());
    ErrPostStr(SEV_REJECT, ERR_FEATURE_RequiredQualifierMissing, msg);

    return false;
}

/**********************************************************/
static bool SortFeaturesByLoc(const DataBlkPtr& sp1, const DataBlkPtr& sp2)
{
    FeatBlkPtr fbp1;
    FeatBlkPtr fbp2;
    Int4       status;

    fbp1 = sp1->GetFeatData();
    fbp2 = sp2->GetFeatData();

    if (! fbp1->location_isset() && fbp2->location_isset())
        return false;
    if (fbp1->location_isset() && ! fbp2->location_isset())
        return false;

    if (fbp1->location_isset() && fbp2->location_isset()) {
        status = StringCmp(fbp1->location_c_str(), fbp2->location_c_str());
        if (status != 0)
            return status < 0;
    }

    if (fbp1->key.empty() && ! fbp2->key.empty())
        return false;
    if (! fbp1->key.empty() && fbp2->key.empty())
        return false;
    if (! fbp1->key.empty() && ! fbp2->key.empty()) {
        status = StringCmp(fbp1->key.c_str(), fbp2->key.c_str());
        if (status != 0)
            return status < 0;
    }

    return false;
}

/**********************************************************/
static bool SortFeaturesByOrder(const DataBlkPtr& sp1, const DataBlkPtr& sp2)
{
    FeatBlkPtr fbp1;
    FeatBlkPtr fbp2;

    fbp1 = sp1->GetFeatData();
    fbp2 = sp2->GetFeatData();

    return fbp1->num < fbp2->num;
}

/**********************************************************/
static DataBlkPtr fta_sort_features(DataBlkPtr dbp, bool order)
{
    size_t total = 0;
    for (DataBlkPtr tdbp = dbp; tdbp; tdbp = tdbp->mpNext)
        total++;

    vector<DataBlk*> temp;
    temp.reserve(total);
    for (DataBlkPtr tdbp = dbp; tdbp; tdbp = tdbp->mpNext)
        temp.push_back(tdbp);

    std::sort(temp.begin(), temp.end(), (order ? SortFeaturesByOrder : SortFeaturesByLoc));

    DataBlkPtr tdbp = dbp = temp[0];
    for (size_t i = 0; i < total - 1; tdbp = tdbp->mpNext, i++)
        tdbp->mpNext = temp[i + 1];

    temp[total - 1]->mpNext = nullptr;
    return (dbp);
}

/**********************************************************/
static void fta_convert_to_regulatory(FeatBlkPtr fbp, const char* rclass)
{
    if (! fbp || fbp->key.empty() || ! rclass)
        return;

    fbp->key = "regulatory";

    CRef<CGb_qual> qual(new CGb_qual);
    qual->SetQual("regulatory_class");
    qual->SetVal(rclass);
    fbp->quals.push_back(qual);
}

/**********************************************************/
static void fta_check_replace_regulatory(DataBlkPtr dbp, bool* drop)
{
    FeatBlkPtr   fbp;
    const char** b;
    const char*  p;
    bool         got_note;
    bool         other_class;
    Int4         count;

    for (; dbp; dbp = dbp->mpNext) {
        fbp = dbp->GetFeatData();
        if (! fbp || fbp->key.empty())
            continue;

        if (fbp->key == "attenuator")
            fta_convert_to_regulatory(fbp, "attenuator");
        else if (fbp->key == "CAAT_signal")
            fta_convert_to_regulatory(fbp, "CAAT_signal");
        else if (fbp->key == "enhancer")
            fta_convert_to_regulatory(fbp, "enhancer");
        else if (fbp->key == "GC_signal")
            fta_convert_to_regulatory(fbp, "GC_signal");
        else if (fbp->key == "-35_signal")
            fta_convert_to_regulatory(fbp, "minus_35_signal");
        else if (fbp->key == "-10_signal")
            fta_convert_to_regulatory(fbp, "minus_10_signal");
        else if (fbp->key == "polyA_signal")
            fta_convert_to_regulatory(fbp, "polyA_signal_sequence");
        else if (fbp->key == "promoter")
            fta_convert_to_regulatory(fbp, "promoter");
        else if (fbp->key == "RBS")
            fta_convert_to_regulatory(fbp, "ribosome_binding_site");
        else if (fbp->key == "TATA_signal")
            fta_convert_to_regulatory(fbp, "TATA_box");
        else if (fbp->key == "terminator")
            fta_convert_to_regulatory(fbp, "terminator");
        else if (fbp->key != "regulatory")
            continue;

        got_note    = false;
        other_class = false;
        count       = 0;

        for (const auto& cur : fbp->quals) {
            if (! cur->IsSetQual() || ! cur->IsSetVal())
                continue;

            const string& qual_str = cur->GetQual();

            if (qual_str != "regulatory_class") {
                if (qual_str == "note")
                    got_note = true;
                continue;
            }

            count++;
            if (! cur->IsSetVal() || cur->GetVal().empty()) {
                if (! fbp->location_isset() || *fbp->location_get() == '\0')
                    p = "(empty)";
                else
                    p = fbp->location;
                auto msg = ErrFormat("Empty /regulatory_class qualifier value in regulatory feature at location %s.", p);
                ErrPostStr(SEV_REJECT, ERR_QUALIFIER_InvalidRegulatoryClass, msg);
                *drop = true;
                continue;
            }

            const string& val_str = cur->GetVal();

            for (b = RegulatoryClassValues; *b; b++)
                if (val_str == *b)
                    break;

            if (*b) {
                if (val_str == "other")
                    other_class = true;
                continue;
            }

            if (! fbp->location_isset() || *fbp->location_get() == '\0')
                p = "(empty)";
            else
                p = fbp->location;
            auto msg = ErrFormat("Invalid /regulatory_class qualifier value %s provided in regulatory feature at location %s.", val_str.c_str(), p);
            ErrPostStr(SEV_REJECT, ERR_QUALIFIER_InvalidRegulatoryClass, msg);
            *drop = true;
        }

        if (count == 0) {
            if (! fbp->location_isset() || *fbp->location_get() == '\0')
                p = "(empty)";
            else
                p = fbp->location;
            auto msg = ErrFormat("The regulatory feature is missing mandatory /regulatory_class qualifier at location %s.", p);
            ErrPostStr(SEV_REJECT, ERR_QUALIFIER_MissingRegulatoryClass, msg);
            *drop = true;
        } else if (count > 1) {
            if (! fbp->location_isset() || *fbp->location_get() == '\0')
                p = "(empty)";
            else
                p = fbp->location;
            auto msg = ErrFormat("Multiple /regulatory_class qualifiers were encountered in regulatory feature at location %s.", p);
            ErrPostStr(SEV_REJECT, ERR_QUALIFIER_MultipleRegulatoryClass, msg);
            *drop = true;
        }

        if (other_class && ! got_note) {
            if (! fbp->location_isset() || *fbp->location_get() == '\0')
                p = "(empty)";
            else
                p = fbp->location;
            auto msg = ErrFormat("The regulatory feature of class other is lacking required /note qualifier at location %s.", p);
            ErrPostStr(SEV_REJECT, ERR_QUALIFIER_NoNoteForOtherRegulatory, msg);
            *drop = true;
        }
    }
}

/**********************************************************/
static void fta_create_wgs_dbtag(CBioseq&      bioseq,
                                 const string& submitter_seqid,
                                 char*         prefix,
                                 Int4          seqtype)
{
    string dbname;
    if (seqtype == 0 || seqtype == 1 || seqtype == 7)
        dbname = "WGS:";
    else if (seqtype == 4 || seqtype == 5 || seqtype == 8 || seqtype == 9)
        dbname = "TSA:";
    else
        dbname = "TLS:";
    dbname += prefix;

    CRef<CSeq_id> gen_id(new CSeq_id);
    CDbtag&       tag = gen_id->SetGeneral();
    tag.SetTag().SetStr(submitter_seqid);
    tag.SetDb(dbname);
    bioseq.SetId().push_back(gen_id);
}

/**********************************************************/
static void fta_create_wgs_seqid(CBioseq&        bioseq,
                                 IndexblkPtr     ibp,
                                 Parser::ESource source)
{
    char*       prefix;
    const char* p;
    Int4        seqtype;
    Int4        i;

    if (! ibp || ibp->submitter_seqid.empty())
        return;

    prefix = nullptr;

    seqtype = fta_if_wgs_acc(ibp->acnum);
    if (seqtype == 0 || seqtype == 3 || seqtype == 4 || seqtype == 6 ||
        seqtype == 10 || seqtype == 12) {
        ErrPostStr(SEV_REJECT, ERR_SOURCE_SubmitterSeqidNotAllowed, "WGS/TLS/TSA master records are not allowed to have /submitter_seqid qualifiers, only contigs and scaffolds. Entry dropped.");
        ibp->drop = true;
        return;
    }

    if (seqtype == 1 || seqtype == 5 || seqtype == 7 || seqtype == 8 ||
        seqtype == 9 || seqtype == 11) {
        prefix = StringSave(ibp->acnum);
        if (prefix[4] >= '0' && prefix[4] <= '9')
            prefix[6] = '\0';
        else
            prefix[8] = '\0';
        fta_create_wgs_dbtag(bioseq, ibp->submitter_seqid, prefix, seqtype);
        MemFree(prefix);
        return;
    }

    bool ok = true;
    for (auto tbp = ibp->secaccs.begin(); tbp != ibp->secaccs.end(); ++tbp) {
        if ((*tbp)[0] == '-')
            continue;

        if (! prefix)
            prefix = StringSave(*tbp);
        else {
            i = (prefix[4] >= '0' && prefix[4] <= '9') ? 6 : 8;
            if (! StringEquN(prefix, tbp->c_str(), i)) {
                ok = false;
                break;
            }
        }
    }

    if (ok && prefix) {
        seqtype = fta_if_wgs_acc(prefix);
        if (seqtype == 0 || seqtype == 1 || seqtype == 4 || seqtype == 5 ||
            seqtype == 7 || seqtype == 8 || seqtype == 9 || seqtype == 10 ||
            seqtype == 11) {
            if (prefix[4] >= '0' && prefix[4] <= '9')
                prefix[6] = '\0';
            else
                prefix[8] = '\0';
            fta_create_wgs_dbtag(bioseq, ibp->submitter_seqid, prefix, seqtype);
            MemFree(prefix);
            return;
        }
    }

    if (prefix) {
        MemFree(prefix);
        prefix = nullptr;
    }

    if (bioseq.GetInst().IsSetExt() && bioseq.GetInst().GetExt().IsDelta()) {
        CDelta_ext::Tdata           deltas = bioseq.GetInst().GetExt().GetDelta();
        CDelta_ext::Tdata::iterator delta;

        for (delta = deltas.begin(); delta != deltas.end(); delta++) {
            const CSeq_id* id = nullptr;

            if (! (*delta)->IsLoc())
                continue;

            const CSeq_loc& locs = (*delta)->GetLoc();
            CSeq_loc_CI     ci(locs);

            for (; ci; ++ci) {
                CConstRef<CSeq_loc> loc = ci.GetRangeAsSeq_loc();
                if (! loc->IsInt())
                    continue;
                id = &ci.GetSeq_id();
                if (! id)
                    break;
                if (! id->IsGenbank() && ! id->IsEmbl() && ! id->IsDdbj() &&
                    ! id->IsOther() && ! id->IsTpg() && ! id->IsTpe() &&
                    ! id->IsTpd())
                    break;

                const CTextseq_id* text_id = id->GetTextseq_Id();
                if (! text_id || ! text_id->IsSetAccession() ||
                    text_id->GetAccession().empty())
                    break;

                p = text_id->GetAccession().c_str();
                if (! prefix)
                    prefix = StringSave(p);
                else {
                    i = (prefix[4] >= '0' && prefix[4] <= '9') ? 6 : 8;
                    if (! StringEquN(prefix, p, i))
                        break;
                }
            }
            if (ci)
                break;
        }

        if (delta == deltas.end() && prefix) {
            seqtype = fta_if_wgs_acc(prefix);
            if (seqtype == 0 || seqtype == 1 || seqtype == 4 || seqtype == 5 ||
                seqtype == 7 || seqtype == 8 || seqtype == 9 || seqtype == 10 ||
                seqtype == 11) {
                if (prefix[4] >= '0' && prefix[4] <= '9')
                    prefix[6] = '\0';
                else
                    prefix[8] = '\0';
                fta_create_wgs_dbtag(bioseq, ibp->submitter_seqid, prefix, seqtype);
                MemFree(prefix);
                return;
            }
        }

        if (prefix) {
            MemFree(prefix);
            prefix = nullptr;
        }

        ErrPostStr(SEV_ERROR, ERR_SOURCE_SubmitterSeqidDropped, "Could not determine project code for what appears to be a WGS/TLS/TSA scaffold record. /submitter_seqid dropped.");
        return;
    }

    if ((source == Parser::ESource::EMBL || source == Parser::ESource::DDBJ) && ibp->is_tsa) {
        auto msg = ErrFormat("Submitter sequence identifiers for non-project-based TSA records are not supported. /submitter_seqid \"%s\" has been dropped.", ibp->submitter_seqid.c_str());
        ErrPostStr(SEV_ERROR, ERR_SOURCE_SubmitterSeqidIgnored, msg);
        return;
    }

    auto msg = ErrFormat("Only WGS/TLS/TSA related records (contigs and scaffolds) are allowed to have /submitter_seqid qualifier. This \"%s\" is not one of them. Entry dropped.", ibp->acnum);
    ErrPostStr(SEV_REJECT, ERR_SOURCE_SubmitterSeqidNotAllowed, msg);
    ibp->drop = true;
}

/**********************************************************
 *
 *   SeqAnnotPtr LoadFeat(pp, entry, bsp):
 *
 *                                              5-4-93
 *
 **********************************************************/
void LoadFeat(ParserPtr pp, const DataBlk& entry, CBioseq& bioseq)
{
    DataBlkPtr dab;
    DataBlkPtr dabnext;
    DataBlkPtr dbp;
    DataBlkPtr tdbp;
    FeatBlkPtr fbp;

    IndexblkPtr   ibp;
    Int4          col_data;
    Int2          type;
    Int4          i = 0;
    CRef<CSeq_id> pat_seq_id;

    xinstall_gbparse_range_func(pp, flat2asn_range_func);

    ibp = pp->entrylist[pp->curindx];

    CRef<CSeq_id> seq_id =
        MakeAccSeqId(ibp->acnum, pp->seqtype, pp->accver, ibp->vernum);
    if (pp->source == Parser::ESource::USPTO) {
        pat_seq_id                  = new CSeq_id;
        CRef<CPatent_seq_id> pat_id = MakeUsptoPatSeqId(ibp->acnum);
        pat_seq_id->SetPatent(*pat_id);
    }

    if (! seq_id) {
        if (! NStr::IsBlank(ibp->acnum)) {
            seq_id = Ref(new CSeq_id(CSeq_id::e_Local, ibp->acnum));
        } else if (pp->mode == Parser::EMode::Relaxed) {
            seq_id = Ref(new CSeq_id(CSeq_id::e_Local, ibp->locusname));
        }
    }

    TSeqIdList ids;
    ids.push_back(seq_id);

    if (pp->format == Parser::EFormat::GenBank) {
        col_data = ParFlat_COL_DATA;
        type     = ParFlat_FEATURES;
    } else if (pp->format == Parser::EFormat::XML) {
        col_data = 0;
        type     = XML_FEATURES;
    } else {
        col_data = ParFlat_COL_DATA_EMBL;
        type     = ParFlat_FH;
    }

    /* Find feature already isolated in a "block"
     * The key, location and qualifiers will be isolated to
     * a FeatBlk at the first step of ParseFeatureBlock, which
     * parses a single feature at a time.
     *                                          -Karl
     */
    if (pp->format == Parser::EFormat::XML)
        dab = XMLLoadFeatBlk(entry.mOffset, ibp->xip);
    else
        dab = TrackNodeType(entry, type);
    for (dbp = dab; dbp; dbp = dbp->mpNext) {
        if (dbp->mType != type)
            continue;

        /* Parsing each feature subblock to FeatBlkPtr, fbp
         * it also checks semantics of qualifiers and keys
         */
        if (pp->format == Parser::EFormat::XML)
            XMLParseFeatureBlock(pp->debug, dbp->GetSubData(), pp->source);
        else
            ParseFeatureBlock(ibp, pp->debug, dbp->GetSubData(), pp->source, pp->format);

        dbp->SetSubData(fta_sort_features(dbp->GetSubData(), false));
        fta_check_pseudogene_qual(dbp->GetSubData());
        fta_check_old_locus_tags(dbp->GetSubData(), &ibp->drop);
        fta_check_compare_qual(dbp->GetSubData(), ibp->is_tpa);
        tdbp = dbp->GetSubData();
        for (i = 0; tdbp; i++, tdbp = tdbp->mpNext)
            fta_remove_dup_quals(tdbp->GetFeatData());
        fta_remove_dup_feats(dbp->GetSubData());
        for (tdbp = dbp->GetSubData(); tdbp; tdbp = tdbp->mpNext)
            fta_check_rpt_unit_range(tdbp->GetFeatData(), ibp->bases);
        fta_check_multiple_locus_tag(dbp->GetSubData(), &ibp->drop);
        if (ibp->is_tpa || ibp->is_tsa || ibp->is_tls)
            fta_check_non_tpa_tsa_tls_locations(dbp->GetSubData(), ibp);
        fta_check_replace_regulatory(dbp->GetSubData(), &ibp->drop);
        dbp->SetSubData(fta_sort_features(dbp->GetSubData(), true));
    }

    if (i > 1 && ibp->is_mga) {
        ErrPostStr(SEV_REJECT, ERR_FEATURE_MoreThanOneCAGEFeat, "CAGE records are allowed to have only one feature, and it must be the \"source\" one. Entry dropped.");
        ibp->drop = true;
    }

    if (! ibp->drop)
        CollectGapFeats(entry, dab, pp, type);

    TSeqFeatList seq_feats;
    if (! ibp->drop)
        ParseSourceFeat(pp, dab, ids, type, bioseq, seq_feats);

    if (seq_feats.empty()) {
        ibp->drop = true;
        for (; dab; dab = dabnext) {
            dabnext = dab->mpNext;
            if (dab->hasData()) {
                FreeFeatBlk(dab->GetSubData(), pp->format);
                dab->mpData.subblocks = nullptr;
                dab->mbSubData        = false;
            }
            if (pp->format == Parser::EFormat::XML)
                dab->SimpleDelete();
        }
        xinstall_gbparse_range_func(nullptr, nullptr);
        return;
    }

    if (! ibp->submitter_seqid.empty())
        fta_create_wgs_seqid(bioseq, ibp, pp->source);

    CSeq_descr::Tdata& descr_list = bioseq.SetDescr().Set();
    for (CSeq_descr::Tdata::iterator descr = descr_list.begin(); descr != descr_list.end();) {
        if (! (*descr)->IsSource()) {
            ++descr;
            continue;
        }

        descr = descr_list.erase(descr);
    }

    CRef<CSeqdesc> descr_src(new CSeqdesc);
    descr_src->SetSource(seq_feats.front()->SetData().SetBiosrc());

    descr_list.push_back(descr_src);
    seq_feats.pop_front();

    fta_get_gcode_from_biosource(descr_src->GetSource(), ibp);

    for (; dab; dab = dabnext) {
        dabnext = dab->mpNext;
        if (dab->mType != type) {
            if (pp->format == Parser::EFormat::XML)
                dab->SimpleDelete();
            continue;
        }

        for (dbp = dab->GetSubData(); dbp; dbp = dbp->mpNext) {
            if (dbp->mDrop == true)
                continue;

            fbp = dbp->GetFeatData();
            if (fbp->key == "source" ||
                fbp->key == "assembly_gap" ||
                (fbp->key == "gap" &&
                 pp->source != Parser::ESource::DDBJ && pp->source != Parser::ESource::EMBL))
                continue;

            fta_sort_quals(fbp, pp->qamode);
            CRef<CSeq_feat> feat;
            if (fbp->spindex < 0)
                feat = ProcFeatBlk(pp, fbp, ids);
            else
                feat = SpProcFeatBlk(pp, fbp, ids);
            if (feat.Empty()) {
                if (fbp->key == "CDS") {
                    auto msg = ErrFormat("CDS feature has unparsable location. Entry dropped. Location = [%s].", fbp->location_c_str());
                    ErrPostStr(SEV_ERROR, ERR_FEATURE_LocationParsing, msg);
                    ibp->drop = true;
                }
                continue;
            }

            if (fbp->key == "mobile_element" &&
                ! fta_check_mobile_element(*feat)) {
                ibp->drop = true;
                continue;
            }

            fta_check_artificial_location(*feat, fbp->key);

            if (CheckForeignLoc(feat->GetLocation(),
                                (pp->source == Parser::ESource::USPTO) ? *pat_seq_id : *seq_id)) {
                auto msg = ErrFormat("Location pointing outside the entry [%s]", fbp->location_c_str());
                ErrPostStr(SEV_WARNING, ERR_LOCATION_FailedCheck, msg);

                if (feat->GetData().IsImp()) {
                    const CImp_feat& imp_feat = feat->GetData().GetImp();
                    if (imp_feat.GetKey() == "intron" ||
                        imp_feat.GetKey() == "exon") {
                        /* foreign introns and exons wouldn't be parsed
                         */
                        feat.Reset();
                        continue;
                    }
                }
            }

            FilterDb_xref(*feat, pp->source);

            i = FTASeqLocCheck(feat->GetLocation(), ibp->acnum);
            if (i == 0) {
                ErrPostStr(SEV_WARNING, ERR_LOCATION_FailedCheck, fbp->location_get());

                if (pp->debug)
                    seq_feats.push_back(feat);
                else {
                    feat.Reset();
                    continue;
                }
            } else {
                if (i == 1) {
                    if (feat->IsSetExcept_text() && feat->GetExcept_text() == "trans-splicing") {
                        auto msg = ErrFormat("Mixed strands in SeqLoc of /trans_splicing feature: %s", fbp->location_c_str());
                        ErrPostStr(SEV_INFO, ERR_LOCATION_TransSpliceMixedStrand, msg);
                    } else {
                        auto msg = ErrFormat("Mixed strands in SeqLoc: %s", fbp->location_c_str());
                        ErrPostStr(SEV_WARNING, ERR_LOCATION_MixedStrand, msg);
                    }
                }

                seq_feats.push_back(feat);
            }
        }
        FreeFeatBlk(dab->GetSubData(), pp->format);
        if (pp->format == Parser::EFormat::XML)
            dab->SimpleDelete();
    }

    if (! fta_perform_operon_checks(seq_feats, ibp)) {
        ibp->drop = true;
        seq_feats.clear();
        xinstall_gbparse_range_func(nullptr, nullptr);
        return;
    }

    bool stop = false;
    for (auto& feat : seq_feats) {
        if (! feat->GetData().IsImp())
            continue;

        const CImp_feat& imp_feat = feat->GetData().GetImp();

        if (imp_feat.IsSetKey() &&
            StringStr(imp_feat.GetKey().c_str(), "RNA")) {
            if (imp_feat.GetKey() == "ncRNA" && ! fta_check_ncrna(*feat)) {
                stop = true;
                break;
            }

            GetRnaRef(*feat, bioseq, pp->source, pp->accver);
        }
    }

    if (stop) {
        ibp->drop = true;
        seq_feats.clear();
        xinstall_gbparse_range_func(nullptr, nullptr);
        return;
    }

    SeqFeatPub(pp, entry, seq_feats, ids, col_data, ibp);
    if (seq_feats.empty() && ibp->drop) {
        xinstall_gbparse_range_func(nullptr, nullptr);
        return;
    }

    /* ImpFeatPub() call will be removed in asn 4.0
     */
    ImpFeatPub(pp, entry, seq_feats, *seq_id, col_data, ibp);

    xinstall_gbparse_range_func(nullptr, nullptr);
    if (seq_feats.empty())
        return;

    CRef<CSeq_annot> annot(new CSeq_annot);
    annot->SetData().SetFtable().swap(seq_feats);

    bioseq.SetAnnot().push_back(annot);
}

/**********************************************************/
static CMolInfo::EBiomol GetBiomolFromToks(char* mRNA, char* tRNA, char* rRNA, char* snRNA, char* scRNA, char* uRNA, char* snoRNA)
{
    char* p = nullptr;

    if (mRNA)
        p = mRNA;
    if (! p || (tRNA && tRNA < p))
        p = tRNA;
    if (! p || (rRNA && rRNA < p))
        p = rRNA;
    if (! p || (snRNA && snRNA < p))
        p = snRNA;
    if (! p || (scRNA && scRNA < p))
        p = scRNA;
    if (! p || (uRNA && uRNA < p))
        p = uRNA;
    if (! p || (snoRNA && snoRNA < p))
        p = snoRNA;

    if (p == mRNA)
        return (Seq_descr_GIBB_mol_mRNA);
    if (p == tRNA)
        return (Seq_descr_GIBB_mol_tRNA);
    if (p == rRNA)
        return (Seq_descr_GIBB_mol_rRNA);
    if (p == snRNA || p == uRNA)
        return (Seq_descr_GIBB_mol_snRNA);
    if (p == snoRNA)
        return (Seq_descr_GIBB_mol_snoRNA);
    return (Seq_descr_GIBB_mol_scRNA);
}

/**********************************************************/
void GetFlatBiomol(CMolInfo::TBiomol& biomol, CMolInfo::TTech tech, char* molstr, ParserPtr pp, const DataBlk& entry, const COrg_ref* org_ref)
{
    Int4       genomic;
    char*      offset;
    char       c;
    DataBlkPtr dbp;

    Int2        count;
    Int2        i;
    EntryBlkPtr ebp;
    IndexblkPtr ibp;
    const char* p;

    char* q;
    char* r;
    char* mRNA   = nullptr;
    char* tRNA   = nullptr;
    char* rRNA   = nullptr;
    char* snRNA  = nullptr;
    char* scRNA  = nullptr;
    char* uRNA   = nullptr;
    char* snoRNA = nullptr;
    bool  stage;
    bool  techok;
    bool  same;
    bool  is_syn;

    ebp = entry.GetEntryData();

    CBioseq& bioseq = ebp->seq_entry->SetSeq();
    ibp             = pp->entrylist[pp->curindx];

    if (ibp->is_prot) {
        bioseq.SetInst().SetMol(CSeq_inst::eMol_aa);
        biomol = CMolInfo::eBiomol_peptide;
        return;
    }

    if (StringEqu(ibp->division, "SYN") ||
        (org_ref && org_ref->IsSetOrgname() && org_ref->GetOrgname().IsSetDiv() &&
         org_ref->GetOrgname().GetDiv() == "SYN"))
        is_syn = true;
    else
        is_syn = false;

    r = nullptr;
    c = '\0';
    if (! ibp->moltype.empty()) {
        if (pp->source == Parser::ESource::DDBJ && molstr && StringEquNI(molstr, "PRT", 3))
            return;

        biomol = Seq_descr_GIBB_mol_genomic;
        bioseq.SetInst().SetMol(CSeq_inst::eMol_dna);

        if (molstr) {
            q = molstr;
            r = molstr;
            if (pp->format == Parser::EFormat::EMBL || pp->format == Parser::EFormat::XML)
                while (*r != ';' && *r != '\n' && *r != '\0')
                    r++;
            else {
                while (*r != ';' && *r != ' ' && *r != '\t' && *r != '\n' &&
                       *r != '\0')
                    r++;
                if (r - molstr > 10)
                    r = molstr + 10;
            }
            c  = *r;
            *r = '\0';
            if (q == r)
                q = (char*)"???";
        } else
            q = (char*)"???";

        same = true;
        if (ibp->moltype == "genomic DNA") {
            biomol = Seq_descr_GIBB_mol_genomic;
            bioseq.SetInst().SetMol(CSeq_inst::eMol_dna);

            if (pp->source == Parser::ESource::EMBL) {
                if (NStr::CompareNocase(q, "DNA") != 0 &&
                    NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            } else if (NStr::CompareNocase(q, "DNA") != 0)
                same = false;
        } else if (ibp->moltype == "genomic RNA") {
            biomol = Seq_descr_GIBB_mol_genomic;
            bioseq.SetInst().SetMol(CSeq_inst::eMol_rna);

            if (pp->source == Parser::ESource::EMBL) {
                if (NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            } else if (NStr::CompareNocase(q, "RNA") != 0)
                same = false;
        } else if (ibp->moltype == "mRNA") {
            biomol = Seq_descr_GIBB_mol_mRNA;
            bioseq.SetInst().SetMol(CSeq_inst::eMol_rna);

            if (pp->source == Parser::ESource::EMBL) {
                if (NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            } else if (NStr::CompareNocase(q, "mRNA") != 0)
                same = false;
        } else if (ibp->moltype == "tRNA") {
            biomol = Seq_descr_GIBB_mol_tRNA;
            bioseq.SetInst().SetMol(CSeq_inst::eMol_rna);

            if (pp->source == Parser::ESource::EMBL) {
                if (NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            } else if (NStr::CompareNocase(q, "tRNA") != 0)
                same = false;
        } else if (ibp->moltype == "rRNA") {
            biomol = Seq_descr_GIBB_mol_rRNA;
            bioseq.SetInst().SetMol(CSeq_inst::eMol_rna);

            if (pp->source == Parser::ESource::EMBL) {
                if (NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            } else if (NStr::CompareNocase(q, "rRNA") != 0)
                same = false;
        } else if (ibp->moltype == "snoRNA") {
            biomol = Seq_descr_GIBB_mol_snoRNA;
            bioseq.SetInst().SetMol(CSeq_inst::eMol_rna);

            if (pp->source == Parser::ESource::EMBL) {
                if (NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            } else if (NStr::CompareNocase(q, "snoRNA") != 0)
                same = false;
        } else if (ibp->moltype == "snRNA") {
            biomol = Seq_descr_GIBB_mol_snRNA;
            bioseq.SetInst().SetMol(CSeq_inst::eMol_rna);

            if (pp->source == Parser::ESource::EMBL) {
                if (NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            } else if (NStr::CompareNocase(q, "snRNA") != 0)
                same = false;
        } else if (ibp->moltype == "scRNA") {
            biomol = Seq_descr_GIBB_mol_scRNA;
            bioseq.SetInst().SetMol(CSeq_inst::eMol_rna);

            if (pp->source == Parser::ESource::EMBL) {
                if (NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            } else if (NStr::CompareNocase(q, "scRNA") != 0)
                same = false;
        } else if (ibp->moltype == "pre-RNA") {
            biomol = Seq_descr_GIBB_mol_preRNA;
            bioseq.SetInst().SetMol(CSeq_inst::eMol_rna);

            if (pp->source == Parser::ESource::EMBL) {
                if (NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            } else if (NStr::CompareNocase(q, "RNA") != 0)
                same = false;
        } else if (ibp->moltype == "pre-mRNA") {
            biomol = Seq_descr_GIBB_mol_preRNA;
            bioseq.SetInst().SetMol(CSeq_inst::eMol_rna);

            if (pp->source == Parser::ESource::EMBL) {
                if (NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            } else if (NStr::CompareNocase(q, "RNA") != 0)
                same = false;
        } else if (ibp->moltype == "other RNA") {
            if (is_syn)
                biomol = Seq_descr_GIBB_mol_other_genetic;
            else
                biomol = Seq_descr_GIBB_mol_other;
            bioseq.SetInst().SetMol(CSeq_inst::eMol_rna);

            if (pp->source == Parser::ESource::EMBL) {
                if (NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            } else if (NStr::CompareNocase(q, "RNA") != 0)
                same = false;
        } else if (ibp->moltype == "other DNA") {
            if (is_syn)
                biomol = Seq_descr_GIBB_mol_other_genetic;
            else
                biomol = Seq_descr_GIBB_mol_other;
            bioseq.SetInst().SetMol(CSeq_inst::eMol_dna);

            if (pp->source == Parser::ESource::EMBL) {
                if (NStr::CompareNocase(q, "DNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            } else if (NStr::CompareNocase(q, "DNA") != 0)
                same = false;
        } else if (ibp->moltype == "unassigned RNA") {
            if (is_syn)
                biomol = Seq_descr_GIBB_mol_other_genetic;
            else
                biomol = Seq_descr_GIBB_mol_unknown;
            bioseq.SetInst().SetMol(CSeq_inst::eMol_rna);

            if (pp->source == Parser::ESource::EMBL) {
                if (NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            } else if (NStr::CompareNocase(q, "RNA") != 0)
                same = false;
        } else if (ibp->moltype == "unassigned DNA") {
            if (is_syn)
                biomol = Seq_descr_GIBB_mol_other_genetic;
            else
                biomol = Seq_descr_GIBB_mol_unknown;
            bioseq.SetInst().SetMol(CSeq_inst::eMol_dna);

            if (pp->source == Parser::ESource::EMBL) {
                if (NStr::CompareNocase(q, "DNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            } else if (NStr::CompareNocase(q, "DNA") != 0)
                same = false;
        } else if (ibp->moltype == "viral cRNA") {
            biomol = Seq_descr_GIBB_mol_cRNA;
            bioseq.SetInst().SetMol(CSeq_inst::eMol_rna);

            if (pp->source == Parser::ESource::EMBL) {
                if (NStr::CompareNocase(q, "RNA") != 0 &&
                    NStr::CompareNocase(q, "cRNA") != 0 &&
                    NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            } else if (NStr::CompareNocase(q, "cRNA") != 0)
                same = false;
        } else if (ibp->moltype == "transcribed RNA") {
            biomol = Seq_descr_GIBB_mol_trRNA;
            bioseq.SetInst().SetMol(CSeq_inst::eMol_rna);

            if (pp->source == Parser::ESource::EMBL) {
                if (NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            } else if (NStr::CompareNocase(q, "RNA") != 0)
                same = false;
        } else if (pp->source == Parser::ESource::USPTO &&
                   NStr::CompareNocase(ibp->moltype, "protein") == 0) {
            biomol = CMolInfo::eBiomol_peptide;
            bioseq.SetInst().SetMol(CSeq_inst::eMol_aa);
        } else {
            auto msg = ErrFormat("Invalid /mol_type value \"%s\" provided in source features. Entry dropped.", ibp->moltype.c_str());
            ErrPostStr(SEV_REJECT, ERR_SOURCE_InvalidMolType, msg);
            ibp->drop = true;
            if (molstr)
                *r = c;
            return;
        }

        if (! same) {
            if (ibp->embl_new_ID) {
                auto msg = ErrFormat("Molecule type \"%s\" from the ID line disagrees with \"%s\" from the /mol_type qualifier.", q, ibp->moltype.c_str());
                ErrPostStr(SEV_REJECT, ERR_SOURCE_MolTypesDisagree, msg);
                ibp->drop = true;
                if (molstr)
                    *r = c;
                return;
            }
            auto msg = ErrFormat("Molecule type \"%s\" from the ID/LOCUS line disagrees with \"%s\" from the /mol_type qualifier.", q, ibp->moltype.c_str());
            ErrPostStr(SEV_ERROR, ERR_SOURCE_MolTypesDisagree, msg);
        }

        if ((tech == CMolInfo::eTech_sts || tech == CMolInfo::eTech_htgs_0 ||
             tech == CMolInfo::eTech_htgs_1 || tech == CMolInfo::eTech_htgs_2 ||
             tech == CMolInfo::eTech_htgs_3 || tech == CMolInfo::eTech_wgs ||
             tech == CMolInfo::eTech_survey) &&
            ibp->moltype != "genomic DNA")
            techok = false;
        else if ((tech == CMolInfo::eTech_est || tech == CMolInfo::eTech_fli_cdna ||
                  tech == CMolInfo::eTech_htc) &&
                 ibp->moltype != "mRNA")
            techok = false;
        else
            techok = true;

        if (! techok) {
            if (tech == CMolInfo::eTech_est)
                p = "EST";
            else if (tech == CMolInfo::eTech_fli_cdna)
                p = "fli-cDNA";
            else if (tech == CMolInfo::eTech_htc)
                p = "HTC";
            else if (tech == CMolInfo::eTech_sts)
                p = "STS";
            else if (tech == CMolInfo::eTech_wgs)
                p = "WGS";
            else if (tech == CMolInfo::eTech_tsa)
                p = "TSA";
            else if (tech == CMolInfo::eTech_targeted)
                p = "TLS";
            else if (tech == CMolInfo::eTech_survey)
                p = "GSS";
            else
                p = "HTG";
            auto msg = ErrFormat("Molecule type \"%s\" from the /mol_type qualifier disagrees with this record's sequence type: \"%s\".", ibp->moltype.c_str(), p);
            ErrPostStr(SEV_ERROR, ERR_SOURCE_MolTypeSeqTypeConflict, msg);
        }

        if (molstr)
            *r = c;
        return;
    }

    if (tech == CMolInfo::eTech_est) {
        biomol = Seq_descr_GIBB_mol_mRNA;
        bioseq.SetInst().SetMol(CSeq_inst::eMol_rna);
        return;
    }

    if (pp->source == Parser::ESource::DDBJ || pp->source == Parser::ESource::LANL ||
        pp->source == Parser::ESource::NCBI) {
        biomol = Seq_descr_GIBB_mol_genomic;
        bioseq.SetInst().SetMol(CSeq_inst::eMol_dna);
    } else {
        biomol = CMolInfo::eBiomol_unknown;
        bioseq.SetInst().SetMol(CSeq_inst::eMol_na);
    }

    if (! molstr)
        genomic = -1;
    else {
        genomic = CheckNA(molstr);
        if (genomic < 0 && pp->source == Parser::ESource::DDBJ)
            genomic = CheckNADDBJ(molstr);
    }

    if (genomic < 0 || genomic > 20) {
        if (pp->source == Parser::ESource::EMBL && StringEquNI(molstr, "XXX", 3))
            return;
        if (pp->source == Parser::ESource::DDBJ && StringEquNI(molstr, "PRT", 3))
            return;
        ibp->drop = true;
        q         = molstr;
        c         = '\0';
        if (q) {
            if (pp->format == Parser::EFormat::EMBL)
                while (*q != ';' && *q != '\n' && *q != '\0')
                    q++;
            else {
                while (*q != ';' && *q != ' ' && *q != '\t' && *q != '\n' &&
                       *q != '\0')
                    q++;
                if (q - molstr > 10)
                    q = molstr + 10;
            }

            c  = *q;
            *q = '\0';
        }
        if (pp->source == Parser::ESource::DDBJ)
            p = "DDBJ";
        else if (pp->source == Parser::ESource::EMBL)
            p = "EMBL";
        else if (pp->source == Parser::ESource::LANL)
            p = "LANL";
        else
            p = "NCBI";

        auto msg = ErrFormat("Molecule type \"%s\" from LOCUS/ID line is not legal value for records from source \"%s\". Sequence rejected.", molstr ? molstr : "???", p);
        ErrPostStr(SEV_FATAL, ERR_FORMAT_InvalidMolType, msg);
        if (q)
            *q = c;
        return;
    }

    if (genomic < 2)
        bioseq.SetInst().SetMol(CSeq_inst::eMol_na);
    else if (genomic > 1 && genomic < 6)
        bioseq.SetInst().SetMol(CSeq_inst::eMol_dna);
    else
        bioseq.SetInst().SetMol(CSeq_inst::eMol_rna);

    if (genomic != 6) /* Not just RNA */
    {
        if (genomic < 2) /* "   ", "NA" or "cDNA" */
            biomol = Seq_descr_GIBB_mol_genomic;
        else if (genomic == 2) /* DNA */
            biomol = Seq_descr_GIBB_mol_genomic;
        else if (genomic == 3) /* genomic DNA */
            biomol = Seq_descr_GIBB_mol_genomic;
        else if (genomic == 4) /* other DNA */
        {
            if (is_syn)
                biomol = Seq_descr_GIBB_mol_other_genetic;
            else
                biomol = Seq_descr_GIBB_mol_other;
        } else if (genomic == 5) /* unassigned DNA */
        {
            if (is_syn)
                biomol = Seq_descr_GIBB_mol_other_genetic;
            else
                biomol = Seq_descr_GIBB_mol_unknown;
        } else if (genomic == 7) /* mRNA */
            biomol = Seq_descr_GIBB_mol_mRNA;
        else if (genomic == 8) /* rRNA */
            biomol = Seq_descr_GIBB_mol_rRNA;
        else if (genomic == 9) /* tRNA */
            biomol = Seq_descr_GIBB_mol_tRNA;
        else if (genomic == 10 || genomic == 12) /* uRNA -> snRNA */
            biomol = Seq_descr_GIBB_mol_snRNA;
        else if (genomic == 11) /* scRNA */
            biomol = Seq_descr_GIBB_mol_scRNA;
        else if (genomic == 13) /* snoRNA */
            biomol = Seq_descr_GIBB_mol_snoRNA;
        else if (genomic == 14) /* pre-RNA */
            biomol = Seq_descr_GIBB_mol_preRNA;
        else if (genomic == 15) /* pre-mRNA */
            biomol = Seq_descr_GIBB_mol_preRNA;
        else if (genomic == 16) /* genomic RNA */
            biomol = Seq_descr_GIBB_mol_genomic;
        else if (genomic == 17) /* other RNA */
        {
            if (is_syn)
                biomol = Seq_descr_GIBB_mol_other_genetic;
            else
                biomol = Seq_descr_GIBB_mol_other;
        } else if (genomic == 18) /* unassigned RNA */
        {
            if (is_syn)
                biomol = Seq_descr_GIBB_mol_other_genetic;
            else
                biomol = Seq_descr_GIBB_mol_unknown;
        } else if (genomic == 19 || genomic == 20) /* cRNA or viral cRNA */
            biomol = Seq_descr_GIBB_mol_cRNA;
        return;
    }

    /* Here goes most complicated case with just RNA
     */
    const Char* div = nullptr;
    if (org_ref && org_ref->IsSetOrgname() && org_ref->GetOrgname().IsSetDiv())
        div = org_ref->GetOrgname().GetDiv().c_str();

    if (pp->source != Parser::ESource::EMBL || pp->format != Parser::EFormat::EMBL) {
        biomol = Seq_descr_GIBB_mol_genomic;
        if (! div || ! StringEquN(div, "VRL", 3)) {
            ErrPostStr(SEV_ERROR, ERR_LOCUS_NonViralRNAMoltype, "Genomic RNA implied by presence of RNA moltype, but sequence is non-viral.");
        }
        return;
    }

    count      = 0;
    size_t len = 0;
    offset     = xSrchNodeType(entry, ParFlat_DE, &len);
    if (offset) {
        mRNA   = SrchTheStr(offset, offset + len, "mRNA");
        tRNA   = SrchTheStr(offset, offset + len, "tRNA");
        rRNA   = SrchTheStr(offset, offset + len, "rRNA");
        snRNA  = SrchTheStr(offset, offset + len, "snRNA");
        scRNA  = SrchTheStr(offset, offset + len, "scRNA");
        uRNA   = SrchTheStr(offset, offset + len, "uRNA");
        snoRNA = SrchTheStr(offset, offset + len, "snoRNA");
        if (mRNA)
            count++;
        if (tRNA)
            count++;
        if (rRNA)
            count++;
        if (snRNA || uRNA)
            count++;
        if (scRNA)
            count++;
        if (snoRNA)
            count++;
    }

    /* Non-viral division
     */
    if (! div || ! StringEquN(div, "VRL", 3)) {
        biomol = Seq_descr_GIBB_mol_mRNA;

        if (count > 1) {
            ErrPostStr(SEV_WARNING, ERR_DEFINITION_DifferingRnaTokens, "More than one of mRNA, tRNA, rRNA, snRNA (uRNA), scRNA, snoRNA present in defline.");
        }

        if (tRNA) {
            for (p = tRNA + 4; *p == ' ' || *p == '\t';)
                p++;
            if (*p == '\n') {
                p++;
                if (StringEquN(p, "DE   ", 5))
                    p += 5;
            }
            if (StringEquNI(p, "Synthetase", 10))
                return;
        }

        if (count > 0)
            biomol = GetBiomolFromToks(mRNA, tRNA, rRNA, snRNA, scRNA, uRNA, snoRNA);
        return;
    }

    /* Viral division
     */
    if (org_ref && org_ref->IsSetOrgname() && org_ref->GetOrgname().IsSetLineage() &&
        StringIStr(org_ref->GetOrgname().GetLineage().c_str(), "no DNA stage"))
        stage = true;
    else
        stage = false;

    dbp = TrackNodeType(entry, ParFlat_FH);
    if (! dbp)
        return;
    dbp = dbp->GetSubData();
    for (i = 0; dbp && i < 2; dbp = dbp->mpNext) {
        if (! dbp->mOffset)
            continue;
        offset = dbp->mOffset + ParFlat_COL_FEATKEY;
        if (StringEquN(offset, "CDS", 3))
            i++;
    }
    if (i > 1) {
        biomol = Seq_descr_GIBB_mol_genomic;
        if (! stage) {
            ErrPostStr(SEV_WARNING, ERR_SOURCE_GenomicViralRnaAssumed, "This sequence is assumed to be genomic due to multiple coding region but lack of a DNA stage is not indicated in taxonomic lineage.");
        }
        return;
    }

    if (count == 0) {
        biomol = Seq_descr_GIBB_mol_genomic;
        if (! stage) {
            ErrPostStr(SEV_ERROR, ERR_SOURCE_UnclassifiedViralRna, "Cannot determine viral molecule type (genomic vs a specific type of RNA) based on definition line, CDS content, or taxonomic lineage. So this sequence has been classified as genomic by default (perhaps in error).");
        } else {
            ErrPostStr(SEV_WARNING, ERR_SOURCE_LineageImpliesGenomicViralRna, "This sequence lacks indication of specific RNA type in the definition line, but the taxonomic lineage mentions lack of a DNA stage, so it is classified as genomic.");
        }
        return;
    }

    if (count > 1) {
        ErrPostStr(SEV_WARNING, ERR_DEFINITION_DifferingRnaTokens, "More than one of mRNA, tRNA, rRNA, snRNA (uRNA), scRNA, snoRNA present in defline.");
    }

    biomol = GetBiomolFromToks(mRNA, tRNA, rRNA, snRNA, scRNA, uRNA, snoRNA);
}

END_NCBI_SCOPE
