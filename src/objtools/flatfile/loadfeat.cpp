/* loadfeat.cpp
 *
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
 * File Name:  loadfeat.c 
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen 
 *
 * File Description:
 * -----------------
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

#define Seq_descr_GIBB_mol_unknown       0
#define Seq_descr_GIBB_mol_genomic       1
#define Seq_descr_GIBB_mol_preRNA        2
#define Seq_descr_GIBB_mol_mRNA          3
#define Seq_descr_GIBB_mol_rRNA          4
#define Seq_descr_GIBB_mol_tRNA          5
#define Seq_descr_GIBB_mol_uRNA          6
#define Seq_descr_GIBB_mol_snRNA         6
#define Seq_descr_GIBB_mol_scRNA         7
#define Seq_descr_GIBB_mol_other_genetic 9
#define Seq_descr_GIBB_mol_cRNA          11
#define Seq_descr_GIBB_mol_snoRNA        12
#define Seq_descr_GIBB_mol_trRNA         13
#define Seq_descr_GIBB_mol_other         255

typedef struct _trna_aa {
    const char *name;
    Uint1      aa;
} TrnaAa, *TrnaAaPtr;

typedef struct _str_num {
    const char *str;
    Int4       num;
} StrNum, *StrNumPtr;

TrnaAa taa[] = {
    {"alanine",        'A'},
    {"arginine",       'R'},
    {"asparagine",     'N'},
    {"aspartic acid",  'D'},
    {"aspartate",      'D'},
    {"cysteine",       'C'},
    {"glutamine",      'Q'},
    {"glutamic acid",  'E'},
    {"glutamate",      'E'},
    {"glycine",        'G'},
    {"histidine",      'H'},
    {"isoleucine",     'I'},
    {"leucine",        'L'},
    {"lysine",         'K'},
    {"methionine",     'M'},
    {"phenylalanine",  'F'},
    {"proline",        'P'},
    {"selenocysteine", 'U'},
    {"serine",         'S'},
    {"threonine",      'T'},
    {"tryptophan",     'W'},
    {"tyrosine",       'Y'},
    {"valine",         'V'},
    {NULL,             '\0'}
};

typedef struct _aa_codons {
    const char *straa;
    Uint1      intaa;
    Uint1      gencode;
    Int4       vals[8];
} AaCodons, *AaCodonsPtr;

AaCodons aacodons[] = {
   {"Ala",    'A',  0, {52, 53, 54, 55, -1, -1, -1, -1}},  /* GCT, GCC, GCA, GCG */
   {"Arg",    'R',  2, {28, 29, 30, 31, -1, -1, -1, -1}},  /* CGT, CGC, CGA, CGG */
   {"Arg",    'R',  5, {28, 29, 30, 31, -1, -1, -1, -1}},  /* CGT, CGC, CGA, CGG */
   {"Arg",    'R',  9, {28, 29, 30, 31, -1, -1, -1, -1}},  /* CGT, CGC, CGA, CGG */
   {"Arg",    'R', 13, {28, 29, 30, 31, -1, -1, -1, -1}},  /* CGT, CGC, CGA, CGG */
   {"Arg",    'R', 14, {28, 29, 30, 31, -1, -1, -1, -1}},  /* CGT, CGC, CGA, CGG */
   {"Arg",    'R',  0, {28, 29, 30, 31, 46, 47, -1, -1}},  /* CGT, CGC, CGA, CGG, AGA, AGG */
   {"Asn",    'N',  9, {40, 41, 42, -1, -1, -1, -1, -1}},  /* AAT, AAC, AAA */
   {"Asn",    'N', 14, {40, 41, 42, -1, -1, -1, -1, -1}},  /* AAT, AAC, AAA */
   {"Asn",    'N',  0, {40, 41, -1, -1, -1, -1, -1, -1}},  /* AAT, AAC */
   {"Asp",    'D',  0, {56, 57, -1, -1, -1, -1, -1, -1}},  /* GAT, GAC */
   {"Asx",    'B',  9, {40, 41, 42, 56, 57, -1, -1, -1}},  /* Asn + Asp */
   {"Asx",    'B', 14, {40, 41, 42, 56, 57, -1, -1, -1}},  /* Asn + Asp */
   {"Asx",    'B',  0, {40, 41, 56, 57, -1, -1, -1, -1}},  /* Asn + Asp */
   {"Cys",    'C', 10, {12, 13, 14, -1, -1, -1, -1, -1}},  /* TGT, TGC, TGA */
   {"Cys",    'C',  0, {12, 13, -1, -1, -1, -1, -1, -1}},  /* TGT, TGC */
   {"Gln",    'Q',  6, {10, 11, 26, 27, -1, -1, -1, -1}},  /* TAA, TAG, CAA, CAG */
   {"Gln",    'Q', 15, {11, 26, 27, -1, -1, -1, -1, -1}},  /* TAG, CAA, CAG */
   {"Gln",    'Q',  0, {26, 27, -1, -1, -1, -1, -1, -1}},  /* CAA, CAG */
   {"Glu",    'E',  0, {58, 59, -1, -1, -1, -1, -1, -1}},  /* GAA, GAG */
   {"Glx",    'Z',  6, {10, 11, 26, 27, 58, 59, -1, -1}},  /* Gln + Glu */
   {"Glx",    'Z',  0, {11, 26, 27, 58, 59, -1, -1, -1}},  /* Gln + Glu */
   {"Glx",    'Z',  0, {26, 27, 58, 59, -1, -1, -1, -1}},  /* Gln + Glu */
   {"Gly",    'G', 13, {46, 47, 60, 61, 62, 63, -1, -1}},  /* AGA, AGG, GGT, GGC, GGA, GGG */
   {"Gly",    'G',  0, {60, 61, 62, 63, -1, -1, -1, -1}},  /* GGT, GGC, GGA, GGG */
   {"His",    'H',  0, {24, 25, -1, -1, -1, -1, -1, -1}},  /* CAT, CAC */
   {"Ile",    'I',  2, {32, 33, -1, -1, -1, -1, -1, -1}},  /* ATT, ATC */
   {"Ile",    'I',  3, {32, 33, -1, -1, -1, -1, -1, -1}},  /* ATT, ATC */
   {"Ile",    'I',  5, {32, 33, -1, -1, -1, -1, -1, -1}},  /* ATT, ATC */
   {"Ile",    'I', 13, {32, 33, -1, -1, -1, -1, -1, -1}},  /* ATT, ATC */
   {"Ile",    'I',  0, {32, 33, 34, -1, -1, -1, -1, -1}},  /* ATT, ATC, ATA */
   {"Leu",    'L',  3, { 2,  3, -1, -1, -1, -1, -1, -1}},  /* TTA, TTG */
   {"Leu",    'L', 12, { 2,  3, 16, 17, 18, -1, -1, -1}},  /* TTA, TTG, CTT, CTC, CTA */
   {"Leu",    'L',  0, { 2,  3, 16, 17, 18, 19, -1, -1}},  /* TTA, TTG, CTT, CTC, CTA, CTG */
   {"Lys",    'K',  9, {43, -1, -1, -1, -1, -1, -1, -1}},  /* AAG */
   {"Lys",    'K', 14, {43, -1, -1, -1, -1, -1, -1, -1}},  /* AAG */
   {"Lys",    'K',  0, {42, 43, -1, -1, -1, -1, -1, -1}},  /* AAA, AAG */
   {"Met",    'M',  2, {34, 35, -1, -1, -1, -1, -1, -1}},  /* ATA, ATG */
   {"Met",    'M',  3, {34, 35, -1, -1, -1, -1, -1, -1}},  /* ATA, ATG */
   {"Met",    'M',  5, {34, 35, -1, -1, -1, -1, -1, -1}},  /* ATA, ATG */
   {"Met",    'M', 13, {34, 35, -1, -1, -1, -1, -1, -1}},  /* ATA, ATG */
   {"Met",    'M',  0, {35, -1, -1, -1, -1, -1, -1, -1}},  /* ATG */
   {"fMet",   'M',  2, {34, 35, -1, -1, -1, -1, -1, -1}},  /* ATA, ATG */
   {"fMet",   'M',  3, {34, 35, -1, -1, -1, -1, -1, -1}},  /* ATA, ATG */
   {"fMet",   'M',  5, {34, 35, -1, -1, -1, -1, -1, -1}},  /* ATA, ATG */
   {"fMet",   'M', 13, {34, 35, -1, -1, -1, -1, -1, -1}},  /* ATA, ATG */
   {"fMet",   'M',  0, {35, -1, -1, -1, -1, -1, -1, -1}},  /* ATG */
   {"Phe",    'F',  0, { 0,  1, -1, -1, -1, -1, -1, -1}},  /* TTT, TTC */
   {"Pro",    'P',  0, {20, 21, 22, 23, -1, -1, -1, -1}},  /* CCT, CCC, CCA, CCG */
   {"Sec",    'U',  0, {-1, -1, -1, -1, -1, -1, -1, -1}},
   {"Ser",    'S',  5, { 4,  5,  6,  7, 44, 45, 46, 47}},  /* TCT, TCC, TCA, TCG, AGT, AGC, AGA, AGG */
   {"Ser",    'S',  9, { 4,  5,  6,  7, 44, 45, 46, 47}},  /* TCT, TCC, TCA, TCG, AGT, AGC, AGA, AGG */
   {"Ser",    'S', 12, { 4,  5,  6,  7, 19, 44, 45, -1}},  /* TCT, TCC, TCA, TCG, CTG, AGT, AGC */
   {"Ser",    'S', 14, { 4,  5,  6,  7, 44, 45, 46, 47}},  /* TCT, TCC, TCA, TCG, AGT, AGC, AGA, AGG */
   {"Ser",    'S',  0, { 4,  5,  6,  7, 44, 45, -1, -1}},  /* TCT, TCC, TCA, TCG, AGT, AGC */
   {"Thr",    'T',  3, {16, 17, 18, 19, 36, 37, 38, 39}},  /* CTT, CTC, CTA, CTG, ACT, ACC, ACA, ACG */
   {"Thr",    'T',  0, {36, 37, 38, 39, -1, -1, -1, -1}},  /* ACT, ACC, ACA, ACG */
   {"Trp",    'W',  1, {15, -1, -1, -1, -1, -1, -1, -1}},  /* TGG */
   {"Trp",    'W',  6, {15, -1, -1, -1, -1, -1, -1, -1}},  /* TGG */
   {"Trp",    'W', 10, {15, -1, -1, -1, -1, -1, -1, -1}},  /* TGG */
   {"Trp",    'W', 11, {15, -1, -1, -1, -1, -1, -1, -1}},  /* TGG */
   {"Trp",    'W', 12, {15, -1, -1, -1, -1, -1, -1, -1}},  /* TGG */
   {"Trp",    'W', 15, {15, -1, -1, -1, -1, -1, -1, -1}},  /* TGG */
   {"Trp",    'W',  0, {14, 15, -1, -1, -1, -1, -1, -1}},  /* TGA, TGG */
   {"Tyr",    'Y', 14, { 8,  9, 10, -1, -1, -1, -1, -1}},  /* TAT, TAC, TAA */
   {"Tyr",    'Y',  0, { 8,  9, -1, -1, -1, -1, -1, -1}},  /* TAT, TAC */
   {"Val",    'V',  0, {48, 49, 50, 51, -1, -1, -1, -1}},  /* GTT, GTC, GTA, GTG */
   {"TERM",   '*',  1, {10, 11, 14, -1, -1, -1, -1, -1}},  /* TAA, TAG, TGA */
   {"TERM",   '*',  2, {10, 11, 46, 47, -1, -1, -1, -1}},  /* TAA, TAG, AGA, AGG */
   {"TERM",   '*',  6, {14, -1, -1, -1, -1, -1, -1, -1}},  /* TGA */
   {"TERM",   '*', 11, {10, 11, 14, -1, -1, -1, -1, -1}},  /* TAA, TAG, TGA */
   {"TERM",   '*', 12, {10, 11, 14, -1, -1, -1, -1, -1}},  /* TAA, TAG, TGA */
   {"TERM",   '*', 14, {11, -1, -1, -1, -1, -1, -1, -1}},  /* TAG */
   {"TERM",   '*', 15, {10, 14, -1, -1, -1, -1, -1, -1}},  /* TAA, TGA */
   {"TERM",   '*',  0, {10, 11, -1, -1, -1, -1, -1, -1}},  /* TAA, TAG */
   {"OTHER",  'X',  0, {-1, -1, -1, -1, -1, -1, -1, -1}},
   {NULL,    '\0',  0, {-1, -1, -1, -1, -1, -1, -1, -1}}
};

static const char *trna_tags[] = {
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
    NULL
};

const char *ParFlat_ESTmod[] = {
    "EST",
    "expressed sequence tag",
    "partial cDNA sequence",
    "transcribed sequence fragment",
    "TSR",
    "putatively transcribed partial sequence",
    "UK putts",
    "Plastid",
    NULL
};

static const char *ParFlat_RNA_array[] = {
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
    NULL
};

static const char *DbxrefTagAny[] = {
    "ASAP",
    "CDD",
    "DBEST",
    "DBSTS",
    "GDB",
    "HMP",
    "MAIZEGDB",
    NULL
};

static const char *DbxrefObsolete[] = {
    "BHB",
    "BIOHEALTHBASE",
    "GENEW",
    "IFO",
    "SWISS-PROT",
    "SPTREMBL",
    "TREMBL",
    NULL
};

static const char *EMBLDbxrefTagStr[] = {
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
    NULL
};

static const char *DbxrefTagStr[] = {
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
    NULL
};

static const char *DbxrefTagInt[] = {
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
    NULL
};

static const char *EmptyQuals[] = {
    "artificial_location",              /* Fake. Put here to catch
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
    "mobile_element_type",              /* Fake. Put here to catch
                                           it's empty */
    "partial",
    "proviral",
    "pseudo",
    "rearranged",
    "ribosomal_slippage",
    "trans_splicing",
    "transgenic",
    "virion",
    NULL
};

const char *TransSplicingFeats[] = {
    "3'UTR",
    "5'UTR",
    "CDS",
    "gene",
    "mRNA",
    "misc_RNA",
    "precursor_RNA",
    "tRNA",
    NULL
};

const char *ncRNA_class_values[] = {
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
    NULL
};

const char *SatelliteValues[] = {
    "satellite",
    "minisatellite",
    "microsatellite",
    NULL
};

const char *PseudoGeneValues[] = {
    "allelic",
    "processed",
    "unitary",
    "unknown",
    "unprocessed",
    NULL
};

const char *RegulatoryClassValues[] = {
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
    NULL
};

StrNum GapTypeValues[] = {
    {"between scaffolds",         8},   /* contig          */
    {"within scaffold",           9},   /* scaffold        */
    {"telomere",                  6},   /* telomere        */
    {"centromere",                5},   /* centromere      */
    {"short arm",                 3},   /* short-arm       */
    {"heterochromatin",           4},   /* heterochromatin */
    {"repeat within scaffold",    7},   /* repeat          */
    {"repeat between scaffolds",  7},   /* repeat          */
    {"unknown",                   0},   /* unknown         */
    {NULL,                       -1}
};

StrNum LinkageEvidenceValues[] = {
    {"paired-ends",         0},         /* paired-end         */
    {"align genus",         1},         /* align-genus        */
    {"align xgenus",        2},         /* align-xgenus       */
    {"align trnscpt",       3},         /* align-trnscpt      */
    {"within clone",        4},         /* within-clone       */
    {"clone contig",        5},         /* clone-contig       */
    {"map",                 6},         /* map                */
    {"strobe",              7},         /* strobe             */
    {"unspecified",         8},         /* unspecified        */
    {"pcr",                 9},         /* pcr                */
    {"proximity ligation", 10},         /* proximity-ligation */
    {NULL,                 -1}
};

/**********************************************************/
static void FreeFeatBlkQual(FeatBlkPtr fbp)
{
    MemFree(fbp->key);
    MemFree(fbp->location);
    delete fbp;
}

/**********************************************************/
static void FreeFeatBlk(DataBlkPtr dbp, Parser::EFormat format)
{
    DataBlkPtr dbpnext;
    FeatBlkPtr fbp;

    for(; dbp != NULL; dbp = dbpnext)
    {
        dbpnext = dbp->mpNext;
        fbp = (FeatBlkPtr) dbp->mpData;
        if(fbp != NULL)
        {
            FreeFeatBlkQual(fbp);
            dbp->mpData = NULL;
        }
        if(format == Parser::EFormat::XML)
            MemFree(dbp);
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

    for(p = value; *p != '\0'; p++)
        if(*p != ' ')
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
static Int4 flat2asn_range_func(void* pp_ptr, const objects::CSeq_id& id)
{
    ParserPtr pp = reinterpret_cast<ParserPtr>(pp_ptr);

    int          use_indx = pp->curindx;
    char*      acnum;

    Int2         vernum;

#ifdef BIOSEQ_FIND_METHOD

    bsp = BioseqFind(sip);
    if(bsp != NULL)
        return(bsp->length);

    // could try ID0 server
    //
    return(-1);

#else

    const objects::CTextseq_id* text_id = nullptr;
    if (id.IsGenbank() || id.IsEmbl() || id.IsDdbj() || id.IsTpg() ||
        id.IsTpe() || id.IsTpd())
        text_id = id.GetTextseq_Id();

    if (text_id != nullptr)
    {
        Int2 text_id_ver = text_id->IsSetVersion() ? text_id->GetVersion() : numeric_limits<short>::min();
        const std::string& text_id_acc = text_id->GetAccession();
        for (use_indx = 0; use_indx < pp->indx; use_indx++)
        {
            acnum = pp->entrylist[use_indx]->acnum;
            vernum = pp->entrylist[use_indx]->vernum;
            if (text_id_acc == acnum &&
                (pp->accver == false || vernum == text_id_ver))
                break;
        }

        if (use_indx >= pp->indx)
        {
            // entry is not present in this file use remote fetch function
            //use_indx = pp->curindx;
            //
            size_t len = (!pp->ffdb) ? -1 : CheckOutsideEntry(pp, text_id_acc.c_str(), text_id_ver);
            if (len != static_cast<size_t>(-1))
                return static_cast<Int4>(len);

            if (pp->buf == NULL)
            {
                if (pp->farseq)
                    return -1;

                if (pp->accver == false || text_id_ver < 0)
                {
                    Nlm_ErrSetContext("validatr", __FILE__, __LINE__);
                    Nlm_ErrPostEx(SEV_WARNING, ERR_LOCATION_FailedCheck,
                              "Location points to outside entry %s",
                              text_id_acc.c_str());
                }
                else
                {
                    Nlm_ErrSetContext("validatr", __FILE__, __LINE__);
                    Nlm_ErrPostEx(SEV_WARNING, ERR_LOCATION_FailedCheck,
                              "Location points to outside entry %s.%d",
                              text_id_acc.c_str(), text_id_ver);
                }
                return(-1);
            }

            if (*pp->buf == '\0')
                return(-1);

            if (pp->source == Parser::ESource::NCBI || pp->source == Parser::ESource::Refseq)
                ErrPostEx(SEV_WARNING, ERR_LOCATION_NCBIRefersToExternalRecord,
                "Feature location references an interval on another record : %s",
                pp->buf);
            else
                ErrPostEx(SEV_WARNING, ERR_LOCATION_RefersToExternalRecord,
                "Feature location references an interval on another record : %s",
                pp->buf);
            MemFree(pp->buf);
            pp->buf = (char*)MemNew(1);
            *pp->buf = '\0';
            return(-1);
        }
    }
    return static_cast<Int4>(pp->entrylist[use_indx]->bases);

#endif
}

/**********************************************************/
static bool CheckForeignLoc(const objects::CSeq_loc& loc, const objects::CSeq_id& sid)
{
    const objects::CSeq_id& pid = *loc.GetId();

    if (loc.IsMix() || loc.IsEquiv() ||
        sid.Compare(pid) == objects::CSeq_id::e_YES)
        return false;

    return true;
}

/**********************************************************/
static CRef<objects::CDbtag> DbxrefQualToDbtag(const objects::CGb_qual& qual, Parser::ESource source)
{
    CRef<objects::CDbtag> tag;

    if (!qual.IsSetQual() ||
        qual.GetQual() != "db_xref")
        return tag;

    if (!qual.IsSetVal() || qual.GetVal().empty())
    {
        ErrPostEx(SEV_WARNING, ERR_QUALIFIER_EmptyQual,
                  "Found empty /db_xref qualifier. Qualifier dropped.");
        return tag;
    }

    const std::string& val = qual.GetVal();
    if (NStr::CompareNocase(val.c_str(), "taxon") == 0)
        return tag;

    std::string line = val;

    if (StringNICmp(line.c_str(), "MGD:MGI:", 8) == 0)
        line = line.substr(4);

    size_t colon = line.find(':');
    if (colon == std::string::npos)
    {
        ErrPostEx(SEV_ERROR, ERR_QUALIFIER_DbxrefIncorrect,
                  "Badly formatted /db_xref qualifier: \"%s\". Qualifier dropped.",
                  val.c_str());
        return tag;
    }

    std::string tail = line.substr(colon + 1);
    line = line.substr(0, colon);

    if (MatchArrayIString(DbxrefObsolete, line.c_str()) > -1)
    {
        ErrPostEx(SEV_WARNING, ERR_FEATURE_ObsoleteDbXref,
                  "/db_xref type \"%s\" is obsolete.", line.c_str());

        std::string buf;
        if(NStr::CompareNocase(line.c_str(), "BHB") == 0)
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

    if(NStr::CompareNocase(line.c_str(), "UNIPROT/SWISS-PROT") == 0 ||
        NStr::CompareNocase(line.c_str(), "UNIPROT/TREMBL") == 0)
    {
        std::string buf("UniProtKB");
        buf += line.substr(7);

        line = buf;
    }

    const Char* strid = NULL;
    Int4 intid = 0;

    const Char* p = tail.c_str();
    if (MatchArrayIString(DbxrefTagAny, line.c_str()) > -1)
    {
        for(strid = p; *p >= '0' && *p <= '9';)
            p++;
        if(*p == '\0' && *strid != '0')
        {
            intid = atoi(strid);
            strid = NULL;
        }
    }
    else if(MatchArrayIString(DbxrefTagStr, line.c_str()) > -1 ||
            (source == Parser::ESource::EMBL &&
             MatchArrayIString(EMBLDbxrefTagStr, line.c_str()) > -1))
    {
        for(strid = p; *p >= '0' && *p <= '9';)
            p++;
        if(*p == '\0')
        {
            ErrPostEx(SEV_WARNING, ERR_QUALIFIER_DbxrefWrongType,
                      "/db_xref qualifier \"%s\" is supposed to be a string, but its value consists of digits only.",
                      val.c_str());
            if(*strid != '0')
            {
                intid = atoi(strid);
                strid = NULL;
            }
        }
    }
    else if(MatchArrayIString(DbxrefTagInt, line.c_str()) > -1)
    {
        const Char* q = p;
        for(; *q == '0';)
            q++;
        if(*q == '\0')
        {
            ErrPostEx(SEV_ERROR, ERR_QUALIFIER_DbxrefShouldBeNumeric,
                      "/db_xref qual should have numeric value greater than 0: \"%s\". Qualifier dropped.",
                      val.c_str());
            return tag;
        }

        const Char* r = q;
        for(; *r >= '0' && *r <= '9';)
            r++;
        if(*r != '\0')
        {
            ErrPostEx(SEV_ERROR, ERR_QUALIFIER_DbxrefWrongType,
                      "/db_xref qualifier \"%s\" is supposed to be a numeric identifier, but its value includes alphabetic characters. Qualifier dropped.",
                      val.c_str());
            return tag;
        }
        if(*r != '\0' || q != p)
            strid = p;
        else if(NStr::CompareNocase(line.c_str(), "IntrepidBio") == 0 && fta_number_is_huge(q))
            strid = q;
        else
            intid = atoi(q);
    }
    else if(NStr::CompareNocase(line.c_str(), "PID") == 0)
    {
        if(*p != 'e' && *p != 'g' && *p != 'd')
        {
            ErrPostEx(SEV_ERROR, ERR_QUALIFIER_DbxrefIncorrect,
                      "Badly formatted /db_xref qual \"PID\": \"%s\". Qualifier dropped.",
                      val.c_str());
            return tag;
        }

        const Char* q = p + 1;
        for(; *q == '0';)
            q++;

        const Char* r = q;
        for (r = q; *r >= '0' && *r <= '9';)
            r++;
        if(*q == '\0' || *r != '\0')
        {
            ErrPostEx(SEV_ERROR, ERR_QUALIFIER_DbxrefShouldBeNumeric,
                      "/db_xref qual \"PID\" should contain numeric value greater than 0: \"%s\". Qualifier dropped.",
                      val.c_str());
            return tag;
        }
        strid = p;
    }
    else
    {
        ErrPostEx(SEV_ERROR, ERR_QUALIFIER_DbxrefUnknownDBName,
                  "Unknown data base name /db_xref = \"%s\". Qualifier dropped.",
                  val.c_str());
        return tag;
    }


    tag.Reset(new objects::CDbtag);

    tag->SetDb(line);

    if(strid != NULL)
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
static void FilterDb_xref(objects::CSeq_feat& feat, Parser::ESource source)
{
    if (!feat.IsSetQual())
        return;

    objects::CSeq_feat::TDbxref& db_refs = feat.SetDbxref();

    for (objects::CSeq_feat::TQual::iterator qual = feat.SetQual().begin(); qual != feat.SetQual().end(); )
    {
        if (!(*qual)->IsSetQual() || (*qual)->GetQual() != "db_xref")
        {
            /* Just skip this qualifier, it isn't db_xref
             */
            ++qual;
            continue;
        }

        /* Current qualifier is db_xref, process it
         */
        CRef<objects::CDbtag> dbtag = DbxrefQualToDbtag(*(*qual), source);
        if (dbtag.NotEmpty())
        {
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

/**********************************************************
 *
 *   bool GetSeqLocation(sfp, location, ids,
 *                          hard_err, pp, name):
 *
 *      Return locmap = TRUE if mapping location rules not
 *   work, then SeqLocPtr->whole = ids[0].
 *      sfp->location is a SeqLocPtr which is defined
 *   as a ValNodePtr.
 *
 *                                              7-26-93
 *
 **********************************************************/
bool GetSeqLocation(objects::CSeq_feat& feat, char* location, TSeqIdList& ids,
                    bool* hard_err, ParserPtr pp, char* name)
{
    bool    sitesmap;
    bool    locmap = true;
    int        num_errs;

    *hard_err = false;
    num_errs = 0;

    CRef<objects::CSeq_loc> loc = xgbparseint_ver(location, locmap, sitesmap,
                                                              num_errs, ids, pp->accver);

    if (loc.NotEmpty())
    {
        TSeqLocList locs;
        locs.push_back(loc);
        fta_fix_seq_loc_id(locs, pp, location, name, false);

        feat.SetLocation(*loc);
    }

    if (num_errs > 0)
    {
        feat.ResetLocation();
        objects::CSeq_loc& cur_loc = feat.SetLocation();
        cur_loc.SetWhole(*(*ids.begin()));
        *hard_err = true;
    }
    else if(!feat.GetLocation().IsEmpty())
    {
        if (feat.GetLocation().IsMix())
        {
            if (feat.GetLocation().GetMix().Get().size() == 1)
            {
                CRef<objects::CSeq_loc> cur_loc(new objects::CSeq_loc);

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
static char* CheckLocStr(const Char* str)
{
    const Char* ptr;
    const Char* eptr;
    char* location;

    ptr = StringChr(str, ';');
    if(ptr != NULL)
        return StringSave(str);

    for(ptr = str; *ptr != ' ' && *ptr != '\0';)
        ptr++;
    while(*ptr == ' ')
        ptr++;

    eptr = StringChr(str, ')');
    if(eptr == NULL)
        return(NULL);

    while(*eptr == ' ' || *eptr == ')')
        --eptr;

    location = StringSave(std::string(ptr, eptr + 1).c_str());
    return(location);
}

/*****************************************************************************
*
*   bool SeqIntCheckCpp(loc) is instead of C-toolkit 'bool SeqIntCheck(sip)'
*       checks that a seq interval is valid
*
*****************************************************************************/
static bool SeqIntCheckCpp(const objects::CSeq_loc& loc)
{
    Uint4 len = numeric_limits<unsigned int>::max();

    objects::CBioseq_Handle bio_h = GetScope().GetBioseqHandle(*loc.GetId());
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
static bool SeqPntCheckCpp(const objects::CSeq_loc& loc)
{
    Uint4 len = numeric_limits<unsigned int>::max();

    objects::CBioseq_Handle bio_h = GetScope().GetBioseqHandle(*loc.GetId());
    if (bio_h.CanGetInst() && bio_h.CanGetInst_Length())
        len = bio_h.GetBioseqLength();

    return loc.GetPnt().GetPoint() < len;
}

/*****************************************************************************
*
*   bool PackSeqPntCheck(loc) is instead of C-toolkit 'Boolean PackSeqPntCheck (pspp)'
*
*****************************************************************************/
static bool PackSeqPntCheckCpp(const objects::CSeq_loc& loc)
{
    Uint4 len = numeric_limits<unsigned int>::max();

    objects::CBioseq_Handle bio_h = GetScope().GetBioseqHandle(*loc.GetId());
    if (bio_h.CanGetInst() && bio_h.CanGetInst_Length())
        len = bio_h.GetBioseqLength();

    ITERATE(objects::CSeq_loc::TPoints, point, loc.GetPacked_pnt().GetPoints())
    {
        if (*point >= len)
            return false;
    }

    return true;
}

/**********************************************************/
/* returns : 2 = Ok, 1 = mixed strands, 0 = error in location
 */
static Uint1 FTASeqLocCheck(const objects::CSeq_loc& locs, char* accession)
{
    Uint1        strand = 99;
    Uint1        retval = 2;

    objects::CSeq_loc_CI ci(locs);

    bool good = true;
    for (; ci; ++ci)
    {
        CConstRef<objects::CSeq_loc> cur_loc = ci.GetRangeAsSeq_loc();

        const objects::CSeq_id* cur_id = nullptr;

        switch (cur_loc->Which())
        {
        case objects::CSeq_loc::e_Int:
            good = SeqIntCheckCpp(*cur_loc);
            if (good)
                cur_id = cur_loc->GetId();
            break;

        case objects::CSeq_loc::e_Pnt:
            good = SeqPntCheckCpp(*cur_loc);
            if (good)
                cur_id = cur_loc->GetId();
            break;

        case objects::CSeq_loc::e_Packed_pnt:
            good = PackSeqPntCheckCpp(*cur_loc);
            if (good)
                cur_id = cur_loc->GetId();
            break;

        case objects::CSeq_loc::e_Bond:
            if (!cur_loc->GetBond().CanGetA())
                good = false;

            if (good)
                cur_id = cur_loc->GetId();
            break;

        case objects::CSeq_loc::e_Empty:
        case objects::CSeq_loc::e_Whole:
            cur_id = cur_loc->GetId();
            break;

        default:
            continue;
        }

        if (!good)
            break;

        if (accession == nullptr || cur_id == nullptr)
            continue;

        if (!cur_id->IsGenbank() && !cur_id->IsEmbl() && !cur_id->IsPir() &&
            !cur_id->IsSwissprot() && !cur_id->IsOther() && !cur_id->IsDdbj() &&
            !cur_id->IsPrf() && !cur_id->IsTpg() && !cur_id->IsTpe() &&
            !cur_id->IsTpd() && !cur_id->IsGpipe())
            continue;

        const objects::CTextseq_id* text_id = cur_id->GetTextseq_Id();

        if (text_id == nullptr || !text_id->CanGetAccession())
            continue;

        if (text_id->GetAccession() == accession)
        {
            if (strand == 99)
                strand = cur_loc->GetStrand();
            else if (strand != cur_loc->GetStrand())
                retval = 1;
        }
    }

    if (!good)
        return 0;

    return retval;
}

/**********************************************************/
static void fta_strip_aa(char* str)
{
    if(str == NULL || *str == '\0')
        return;

    while(str != NULL)
    {
        str = StringStr(str, "aa");
        if(str != NULL)
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
static void SeqFeatPub(ParserPtr pp, const DataBlk& entry, TSeqFeatList& feats,
                       TSeqIdList& seqids, Int4 col_data, IndexblkPtr ibp)
{
    DataBlkPtr dbp;
    DataBlkPtr subdbp;
    char*    p;
    char*    q;
    char*    location = NULL;

    bool    err = false;
    Uint1      i;

    /* REFERENCE, to Seq-feat
     */
    if(pp->format == Parser::EFormat::XML)
        dbp = XMLBuildRefDataBlk(entry.mOffset, ibp->xip, ParFlat_REF_BTW);
    else
        dbp = TrackNodeType(entry, ParFlat_REF_BTW);
    if(dbp == NULL)
        return;


    for(; dbp != NULL; dbp = dbp->mpNext)
    {
        if(dbp->mType != ParFlat_REF_BTW)
            continue;

        CRef<objects::CPubdesc> pubdesc = DescrRefs(pp, dbp, col_data);
        if (pubdesc.Empty())
            continue;

        CRef<objects::CSeq_feat> feat(new objects::CSeq_feat);
        feat->SetData().SetPub(*pubdesc);

        location = NULL;
        if(pp->format == Parser::EFormat::XML)
        {
            location = XMLFindTagValue(dbp->mOffset, (XmlIndexPtr) dbp->mpData,
                                       INSDREFERENCE_POSITION);
            if(location == NULL)
            {
                q = XMLFindTagValue(dbp->mOffset, (XmlIndexPtr) dbp->mpData,
                                    INSDREFERENCE_REFERENCE);
                if(q != NULL)
                {
                    for(p = q; *p != '\0' && *p != '(';)
                        p++;
                    if(*p != '\0')
                        location = CheckLocStr(p + 1);
                    MemFree(q);
                }
            }
            else
            {
                p = StringChr(location, ';');
                if(p != NULL)
                {
                    p = (char*) MemNew(StringLen(location) + 7);
                    StringCpy(p, "join(");
                    StringCat(p, location);
                    StringCat(p, ")");
                    MemFree(location);
                    location = p;
                }
            }
        }
        else if(pp->format == Parser::EFormat::GenBank)
        {
            for(p = dbp->mOffset + col_data; *p != '\0' && *p != '(';)
                p++;
            location = CheckLocStr(std::string(p, dbp->mOffset + dbp->len - p).c_str());
        }
        else if(pp->format == Parser::EFormat::EMBL)
        {
            subdbp = (DataBlkPtr) dbp->mpData;
            for(; subdbp != NULL; subdbp = subdbp->mpNext)
            {
                if(subdbp->mType != ParFlat_RP)
                    continue;

                for(p = subdbp->mOffset; *p != '\0' && isdigit(*p) == 0;)
                    p++;
                if(StringChr(p, ',') != NULL)
                {
                    location = (char*) MemNew(StringLen(p) + 7);
                    sprintf(location, "join(%s)", p);
                }
                else
                    location = StringSave(p);
                break;
            }
        }
        if(location == NULL || *location == '\0')
        {
            ErrPostEx(SEV_REJECT, ERR_REFERENCE_UnparsableLocation,
                      "NULL or empty reference location. Entry dropped.");
            err = true;
            if(location != NULL)
                MemFree(location);
            break;
        }

        if(ibp->is_prot)
            fta_strip_aa(location);

        if(pp->buf != NULL)
            MemFree(pp->buf);
        pp->buf = NULL;

        GetSeqLocation(*feat, location, seqids, &err, pp, (char*) "pub");

        if(err)
        {
            ErrPostEx(SEV_REJECT, ERR_REFERENCE_UnparsableLocation,
                      "Unparsable reference location. Entry dropped.");
            MemFree(location);
            break;
        }

        i = FTASeqLocCheck(feat->GetLocation(), ibp->acnum);

        if(i == 0)
        {
            ErrPostEx(SEV_WARNING, ERR_LOCATION_FailedCheck, location);
            if(pp->debug)
            {
                feats.push_back(feat);
            }
        }
        else
        {
            if(i == 1)
            {
                ErrPostEx(SEV_WARNING, ERR_LOCATION_MixedStrand,
                          "Mixed strands in SeqLoc: %s", location);
            }
            feats.push_back(feat);
        }
        if(location != NULL)
            MemFree(location);
    }

    if(!err)
        return;

    ibp->drop = 1;
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
static void ImpFeatPub(ParserPtr pp, const DataBlk& entry, TSeqFeatList& feats,
                       objects::CSeq_id& seq_id, Int4 col_data, IndexblkPtr ibp)
{
    DataBlkPtr dbp;

    bool    first;

    /* REFERENCE, Imp-feat
     */
    if(pp->format == Parser::EFormat::XML)
        dbp = XMLBuildRefDataBlk(entry.mOffset, ibp->xip, ParFlat_REF_SITES);
    else
        dbp = TrackNodeType(entry, ParFlat_REF_SITES);
    if(dbp == NULL)
        return;

    CRef<objects::CSeq_feat> feat;
    for (first = true; dbp != NULL; dbp = dbp->mpNext)
    {
        if(dbp->mType != ParFlat_REF_SITES)
            continue;

        CRef<objects::CPubdesc> pubdesc = DescrRefs(pp, dbp, col_data);
        if (pubdesc.Empty() || !pubdesc->IsSetPub())
            continue;

        if(first)
        {
            feat.Reset(new objects::CSeq_feat);

            objects::CImp_feat& imp_feat = feat->SetData().SetImp();
            imp_feat.SetKey("Site-ref");
            imp_feat.SetLoc("sites");

            feat->SetLocation(*fta_get_seqloc_int_whole(seq_id, ibp->bases));
            first = false;
        }

        CRef<objects::CPub> pub(new objects::CPub);
        pub->SetEquiv(pubdesc->SetPub());

        feat->SetCit().SetPub().push_back(pub);

        if (pubdesc->IsSetComment())
            feat->SetComment(pubdesc->GetComment());
        else
            feat->ResetComment();
    }

    if (!first && feat.NotEmpty())
        feats.push_back(feat);
}

/**********************************************************/
static void fta_fake_gbparse_err_handler(const Char*, const Char*)
{
}

/**********************************************************/
string location_to_string_or_unknown(const objects::CSeq_loc& loc)
{
    auto ret = location_to_string(loc);
    if (!ret.empty())
        return ret;

    return "unknown location";
}

/**********************************************************/
static CRef<objects::CSeq_loc> GetTrnaAnticodon(const objects::CSeq_feat& feat, char* qval, const TSeqIdList& seqids,
                                                            bool accver)
{
    char*    loc_str;
    char*    p;
    char*    q;
    bool    fake1;
    bool    fake2;
    Int4       range;
    Int4       pars;
    Char       ch;
    int        fake3;

    CRef<objects::CSeq_loc> ret;

    if (qval == NULL)
        return ret;

    p = StringStr(qval, "pos:");
    if (p == NULL)
        return ret;

    for(q = p + 4; *q == ' ';)
        q++;

    for(pars = 0, p = q; *p != '\0'; p++)
    {
        if(*p == ',' && pars == 0)
            break;
        if(*p == '(')
            pars++;
        else if(*p == ')')
        {
            pars--;
            if(pars == 0)
            {
                p++;
                break;
            }
        }
    }

    ch = *p;
    *p = '\0';
    loc_str = StringSave(q);
    *p = ch;

    xinstall_gbparse_error_handler(fta_fake_gbparse_err_handler);
    ret = xgbparseint_ver(loc_str, fake1, fake2, fake3, seqids, accver);
    xinstall_gbparse_error_handler(NULL);

    if (ret.Empty())
    {
        string loc = location_to_string_or_unknown(feat.GetLocation());

        ErrPostEx(SEV_ERROR, ERR_FEATURE_InvalidAnticodonPos,
                  "Invalid position element for an /anticodon qualifier : \"%s\" : qualifier dropped : feature location \"%s\".",
                  loc_str, (loc.empty()) ? "unknown" : loc.c_str());

        MemFree(loc_str);

        return ret;
    }

    range = objects::sequence::GetLength(*ret, &GetScope());
    if (range != 3)
    {
        string loc = location_to_string_or_unknown(feat.GetLocation());

        if (range == 4)
            ErrPostEx(SEV_WARNING, ERR_FEATURE_FourBaseAntiCodon,
                      "tRNA feature at \"%s\" has anticodon with location spanning four bases: \"%s\". Cannot generate corresponding codon value from the DNA sequence.",
                      loc.empty() ? "unknown" : loc.c_str(), loc_str);
        else
            ErrPostEx(SEV_ERROR, ERR_FEATURE_StrangeAntiCodonSize,
                      "tRNA feature at \"%s\" has anticodon of an unusual size: \"%s\". Cannot generate corresponding codon value from the DNA sequence.",
                loc.empty() ? "unknown" : loc.c_str(), loc_str);
    }

    // Comparing two locations ignoring their IDs
    // Anticodon should be inside the original location (may be the same)
    CRange<TSeqPos> anticodon_range = ret->GetTotalRange();
    CRange<TSeqPos> xrange = feat.GetLocation().GetTotalRange().IntersectionWith(anticodon_range);

    if (xrange != anticodon_range)
    {
        string loc = location_to_string_or_unknown(feat.GetLocation());

        ErrPostEx(SEV_ERROR, ERR_FEATURE_BadAnticodonLoc,
                  "Anticodon location \"%s\" does not fall within tRNA feature at \"%s\".",
                  loc_str, loc.empty() ? "unknown" : loc.c_str());

        MemFree(loc_str);
        ret.Reset();
        return ret;
    }

    MemFree(loc_str);
    return ret;
}

/**********************************************************/
static void fta_parse_rrna_feat(objects::CSeq_feat& feat, objects::CRNA_ref& rna_ref)
{
    char* qval;
    char* p;
    char* q;
    Char    ch;

    qval = GetTheQualValue(feat.SetQual(), "product");
    if (feat.GetQual().empty())
        feat.ResetQual();

    std::string qval_str;
    if (qval)
    {
        qval_str = qval;
        MemFree(qval);
        qval = NULL;
    }

    size_t len = 0;
    if (qval_str.empty() && feat.IsSetComment() && rna_ref.GetType() == objects::CRNA_ref::eType_rRNA)
    {
        std::string comment = feat.GetComment();
        len = comment.size();

        if(len > 15 && len < 20)
        {
            if(StringNICmp(comment.c_str() + len - 15, "S ribosomal RNA", 15) == 0)
            {
                qval_str = comment;
                feat.ResetComment();
            }
        }
        else if(len > 6 && len < 20)
        {
            if (StringNICmp(comment.c_str() + len - 6, "S rRNA", 6) == 0)
            {
                qval_str = comment;
                feat.ResetComment();
            }
        }
    }

    if (qval_str.empty())
        return;

    qval = StringSave(qval_str.c_str());
    for(p = qval; p != NULL; p += 13)
    {
        p = StringIStr(p, "ribosomal rrna");
        if(p == NULL)
            break;
        fta_StringCpy(p + 10, p + 11);
    }

    for(p = qval; p != NULL; p = qval + len)
    {
        p = StringIStr(p, "ribosomalrna");
        if(p == NULL)
            break;
        q = (char*) MemNew(StringLen(qval) + 2);
        p[9] = '\0';
        StringCpy(q, qval);
        StringCat(q, " RNA");
        StringCat(q, p + 12);
        len = p - qval + 13;
        MemFree(qval);
        qval = q;
    }

    if(qval != NULL)
    {
        p = StringIStr(qval, " rrna");
        if(p != NULL)
        {
            q = (char*) MemNew(StringLen(qval) + 10);
            *p = '\0';
            StringCpy(q, qval);
            StringCat(q, " ribosomal RNA");
            StringCat(q, p + 5);
            MemFree(qval);
            qval = q;
        }
    }

    for(p = qval, q = p; q != NULL; q = p + 13)
    {
        p = StringIStr(q, "ribosomal DNA");
        if(p == NULL)
        {
            p = StringIStr(q, "ribosomal RNA");
            if(p == NULL)
                break;
        }
        p[10] = 'R';
        p[11] = 'N';
        p[12] = 'A';
    }

    p = StringIStr(qval, "s ribosomal RNA");
    if(p != NULL && p > qval && p[15] == '\0')
    {
        p--;
        if(*p >= '0' && *p <= '9')
            *++p = 'S';
    }

    for(p = qval;;)
    {
        p = StringIStr(p, "ribosomal");
        if(p == NULL)
            break;
        if(p == qval || (p[9] != ' ' && p[9] != '\0'))
        {
            p += 9;
            continue;
        }
        if(StringNCmp(p + 9, " RNA", 4) == 0)
        {
            p += 13;
            continue;
        }
        len = p - qval + 14;
        q = (char*) MemNew(StringLen(qval) + 5);
        p += 9;
        ch = *p;
        *p = '\0';
        StringCpy(q, qval);
        StringCat(q, " RNA");
        *p = ch;
        StringCat(q, p);
        MemFree(qval);
        qval = q;
        p = qval + len;
    }

    for(p = qval;;)
    {
        p = StringIStr(p, " ribosomal RNA");
        if(p == NULL)
            break;
        p += 14;
        if(StringNICmp(p, " ribosomal RNA", 14) == 0)
            fta_StringCpy(p, p + 14);
    }

    DeleteQual(feat.SetQual(), "product");
    if (feat.GetQual().empty())
        feat.ResetQual();

    if(StringLen(qval) > 511)
    {
        qval[510] = '>';
        qval[511] = '\0';
        p = StringSave(qval);
        MemFree(qval);
        qval = p;
    }

    rna_ref.SetExt().SetName(qval);
    MemFree(qval);
}

/**********************************************************/
static Uint1 fta_get_aa_from_symbol(Char ch)
{
    AaCodonsPtr acp;

    for(acp = aacodons; acp->straa != NULL; acp++)
        if(acp->intaa == ch)
            break;
    if(acp->straa != NULL)
        return(acp->intaa);

    return(0);
}

/**********************************************************/
static Uint1 fta_get_aa_from_string(char* str)
{
    AaCodonsPtr acp;
    TrnaAaPtr   tap;

    for(tap = taa; tap->name != NULL; tap++)
        if(NStr::CompareNocase(str, tap->name) == 0)
            break;
    if(tap->name != NULL)
        return(tap->aa);

    for(acp = aacodons; acp->straa != NULL; acp++)
        if(NStr::CompareNocase(acp->straa, str) == 0)
            break;
    if(acp->straa != NULL)
        return(acp->intaa);

    return(0);
}

/**********************************************************/
static int get_aa_from_trna(const objects::CTrna_ext& trna)
{
    int ret = 0;
    if (trna.IsSetAa() && trna.GetAa().IsNcbieaa())
        ret = trna.GetAa().GetNcbieaa();

    return ret;
}

/**********************************************************/
static CRef<objects::CTrna_ext> fta_get_trna_from_product(objects::CSeq_feat& feat, const Char* product,
                                                                      unsigned char* remove)
{
    const char **b;

    char*    p;
    char*    q;
    char*    start;
    char*    end;
    char*    first;
    char*    second;
    char*    third;
    char*    fourth;
    bool       fmet;
    char*    prod;

    if (remove != NULL)
        *remove = 0;

    CRef<objects::CTrna_ext> ret(new objects::CTrna_ext);

    if(product == NULL || StringLen(product) < 7)
        return ret;

    bool digits = false;
    prod = StringSave(product);
    for(p = prod, q = prod; *p != '\0'; p++)
    {
        if(*p >= 'a' && *p <= 'z')
            *p &= ~040;
        else if((*p < 'A' || *p > 'Z') && *p != '(' && *p != ')')
        {
            if(*p >= '0' && *p <= '9')
                digits = true;
            *p = ' ';
        }
    }
    ShrinkSpaces(prod);

    for(b = trna_tags; *b != NULL; b++)
    {
        start = StringStr(prod, *b);
        if(start != NULL)
            break;
    }
    if(*b == NULL)
    {
        MemFree(prod);
        return ret;
    }

    end = start + StringLen(*b);
    for(p = end; *p != '\0'; p++)
        if(*p == '(' || *p == ')')
            *p = ' ';
    ShrinkSpaces(prod);

    if(start == prod && *end == '\0')
    {
        if(remove != NULL && !digits)
            *remove = 1;
        MemFree(prod);
        return ret;
    }

    first = NULL;
    second = NULL;
    third = NULL;
    fourth = NULL;
    for(p = end; *p == ' ' || *p == ')' || *p == '(';)
        p++;
    q = p;
    if(StringNCmp(p, "F MET", 5) == 0)
        p += 5;
    else if(StringNCmp(p, "F MT", 4) == 0)
        p += 4;
    while(*p >= 'A' && *p <= 'Z')
        p++;
    if(p > q)
    {
        if(*p != '\0')
            *p++ = '\0';
        second = q;
    }
    while(*p == ' ' || *p == ')' || *p == '(')
        p++;
    for(q = p; *p >= 'A' && *p <= 'Z';)
        p++;
    if(p > q)
    {
        if(*p != '\0')
            *p++ = '\0';
        if(q[1] == '\0')
        {
            while(*p == ' ' || *p == ')' || *p == '(')
                p++;
            for(q = p; *p >= 'A' && *p <= 'Z';)
                p++;
            if(p > q)
            {
                if(*p != '\0')
                    *p++ = '\0';
                third = q;
            }
        }
        else
            third = q;

        while(*p == ' ' || *p == '(' || *p == ')')
            p++;
        if(*p != '\0')
            fourth = p;
    }
    if(start > prod)
    {
        for(p = start - 1; *p == ' ' || *p == ')' || *p == '('; p--)
            if(p == prod)
                break;

        if(p > prod && p[1] == ')')
        {
            for(p--; *p != '('; p--)
                if(p == prod)
                    break;
            if(p > prod)
            {
                for(p--; *p == ' ' || *p == '(' || *p == '('; p--)
                    if(p == prod)
                        break;
            }
        }
        if(p > prod)
        {
            for(q = p++; *q >= 'A' && *q <= 'Z'; q--)
                if(q == prod)
                    break;
            if(*q < 'A' || *q > 'Z')
                q++;
            if(p > q)
            {
                *p = '\0';
                first = q;
            }
        }
    }

    fmet = false;
    if(second != NULL)
    {
        if(StringCmp(second, "F MET") == 0 ||
           StringCmp(second, "FMET") == 0 ||
           StringCmp(second, "F MT") == 0)
        {
            StringCpy(second, "FMET");
            fmet = true;
        }

        ret->SetAa().SetNcbieaa(fta_get_aa_from_string(second));
        if (get_aa_from_trna(*ret) != 0)
            second = NULL;
    }
            
    if (get_aa_from_trna(*ret) == 0 && first != NULL)
    {
        ret->SetAa().SetNcbieaa(fta_get_aa_from_string(first));
        if (get_aa_from_trna(*ret) != 0 && first == prod)
            first = NULL;
    }

    if(first == NULL && second == NULL && third == NULL && fourth == NULL &&
       remove != NULL && !digits)
        *remove = 1;
    MemFree(prod);

    if (!fmet)
        return ret;

    if (!feat.IsSetComment())
        feat.SetComment("fMet");
    else if (StringIStr(feat.GetComment().c_str(), "fmet") == NULL)
    {
        std::string& comment = feat.SetComment();
        comment += "; fMet";
    }

    return ret;
}

/**********************************************************/
static CRef<objects::CTrna_ext> fta_get_trna_from_comment(const Char* comment, unsigned char* remove)
{
    char* comm;
    char* p;
    char* q;

    CRef<objects::CTrna_ext> ret(new objects::CTrna_ext);

    *remove = 0;
    if(comment == NULL)
        return ret;

    comm = StringSave(comment);
    for(p = comm, q = comm; *p != '\0'; p++)
    {
        if(*p >= 'a' && *p <= 'z')
            *p &= ~040;
        else if(*p < 'A' || *p > 'Z')
            *p = ' ';
    }
    ShrinkSpaces(comm);

    if(StringNCmp(comm, "CODON RECOGNIZED ", 17) == 0)
    {
        p = comm + 17;
        q = StringChr(p, ' ');
        if(q != NULL && StringCmp(q + 1, "PUTATIVE") == 0)
            *q = '\0';
        if(StringChr(p, ' ') == NULL && StringLen(p) == 3)
        {
            MemFree(comm);
            *remove = (q == NULL) ? 1 : 2;
            return ret;
        }
    }

    if(StringNCmp(comm, "PUTATIVE ", 9) == 0 && comm[10] == ' ' &&
       comm[14] == ' ' && StringNCmp(&comm[15], "TRNA", 4) == 0)
    {
        ret->SetAa().SetNcbieaa(fta_get_aa_from_symbol(comm[9]));
        if (get_aa_from_trna(*ret) != 0)
        {
            MemFree(comm);
            return ret;
        }
    }

    for(q = comm, p = q; p != NULL;)
    {
        p = StringChr(p, ' ');
        if(p != NULL)
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
static int get_first_codon_from_trna(const objects::CTrna_ext& trna)
{
    int ret = 255;
    if (trna.IsSetCodon() && !trna.GetCodon().empty())
        ret = *trna.GetCodon().begin();

    return ret;
}

/**********************************************************/
static void GetRnaRef(objects::CSeq_feat& feat, objects::CBioseq& bioseq,
                      Parser::ESource source, bool accver)
{
    char*    qval;
    char*    p;

    Uint1      remove;

    Int2       type;

    if (!feat.GetData().IsImp())
        return;

    const objects::CImp_feat& imp_feat = feat.GetData().GetImp();

    CRef<objects::CRNA_ref> rna_ref(new objects::CRNA_ref);

    type = MatchArrayString(ParFlat_RNA_array, imp_feat.GetKey().c_str());
    if (type < 0)
        type = 255;
    else
        ++type;

    rna_ref->SetType(static_cast<objects::CRNA_ref::EType>(type));

    feat.SetData().SetRna(*rna_ref);

    if (type == objects::CRNA_ref::eType_rRNA)
    {
        fta_parse_rrna_feat(feat, *rna_ref);
        return;
    }

    CRef<objects::CRNA_gen> rna_gen;
    CRef<objects::CRNA_qual_set> rna_quals;

    if (type == objects::CRNA_ref::eType_ncRNA)
    {
        p = GetTheQualValue(feat.SetQual(), "ncRNA_class");
        if(p != NULL)
        {
            rna_gen.Reset(new objects::CRNA_gen);
            rna_gen->SetClass(p);
        }
    }
    else if (type == objects::CRNA_ref::eType_tmRNA)
    {
        p = GetTheQualValue(feat.SetQual(), "tag_peptide");
        if (p != NULL)
        {
            CRef<objects::CRNA_qual> rna_qual(new objects::CRNA_qual);
            rna_qual->SetQual("tag_peptide");
            rna_qual->SetVal(p);

            rna_quals.Reset(new objects::CRNA_qual_set);
            rna_quals->Set().push_back(rna_qual);

            rna_gen.Reset(new objects::CRNA_gen);
            rna_gen->SetQuals(*rna_quals);
        }
    }

    if (type != objects::CRNA_ref::eType_premsg && type != objects::CRNA_ref::eType_tRNA)    /* mRNA, snRNA, scRNA or other */
    {
        qval = GetTheQualValue(feat.SetQual(), "product");
        if(qval != NULL)
        {
            p = GetTheQualValue(feat.SetQual(), "product");
            if(p != NULL && p[0] != 0)
            {
                if (!feat.IsSetComment())
                    feat.SetComment(p);
                else
                {
                    std::string& comment = feat.SetComment();
                    comment += "; ";
                    comment += p;
                }
            }
        }

        if (qval == NULL && type == objects::CRNA_ref::eType_mRNA &&
           source != Parser::ESource::EMBL && source != Parser::ESource::DDBJ)
           qval = GetTheQualValue(feat.SetQual(), "standard_name");

        if (qval == NULL && feat.IsSetComment() && type == objects::CRNA_ref::eType_mRNA)
        {
            const Char* c_p = feat.GetComment().c_str();
            const Char* c_q = NULL;
            for ( ; ; c_p += 5, c_q = c_p)
            {
                c_p = StringIStr(c_p, " mRNA");
                if (c_p == NULL)
                    break;
            }

            const Char* c_r = NULL;
            for (c_p = feat.GetComment().c_str(); ; c_p += 4, c_r = c_p)
            {
                c_p = StringIStr(c_p, " RNA");
                if (c_p == NULL)
                    break;
            }

            if (c_q != NULL && c_r != NULL)
            {
                c_p = (c_q > c_r) ? c_q : c_r;
            }
            else if (c_q != NULL)
                c_p = c_q;
            else
                c_p = c_r;

            if (c_p != NULL)
            {
                while (*c_p == ' ' || *c_p == '\t' || *c_p == ',' || *c_p == ';')
                    ++c_p;

                if (*c_p == '\0')
                {
                    qval = StringSave(feat.GetComment().c_str());
                    feat.ResetComment();
                }
            }
        }

        if (qval != NULL)
        {
            if(StringLen(qval) > 511)
            {
                qval[510] = '>';
                qval[511] = '\0';
                p = StringSave(qval);
                MemFree(qval);
                qval = p;
            }

            if (type > objects::CRNA_ref::eType_snoRNA && type <= objects::CRNA_ref::eType_miscRNA)
            {
                if (rna_gen.Empty())
                    rna_gen.Reset(new objects::CRNA_gen);

                rna_gen->SetProduct(qval);
            }
            else
            {
                rna_ref->SetExt().SetName(qval);
            }
        }
    }

    if (feat.GetQual().empty())
        feat.ResetQual();

    if (rna_gen.NotEmpty())
    {
        rna_ref->SetExt().SetGen(*rna_gen);
    }

    if (type != objects::CRNA_ref::eType_tRNA)                  /* if tRNA and codon value exist */
        return;

    qval = GetTheQualValue(feat.SetQual(), "anticodon");
    CRef<objects::CTrna_ext> trnaa;
    if (qval != NULL)
    {
        bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_na);

        CRef<objects::CSeq_loc> anticodon = GetTrnaAnticodon(feat, qval, bioseq.GetId(), accver);
        if (anticodon.NotEmpty())
        {
            trnaa.Reset(new objects::CTrna_ext);

            /* value has format: (pos:base_range, aa:amino_acid)
             */
            trnaa->SetAa().SetNcbieaa(GetQualValueAa(qval, true));
            trnaa->SetAnticodon(*anticodon);
            rna_ref->SetExt().SetTRNA(*trnaa);
        }

        MemFree(qval);
    }

    qval = CpTheQualValue(feat.SetQual(), "product");

    CRef<objects::CTrna_ext> trnap;
    if (qval != NULL)
    {
        trnap = fta_get_trna_from_product(feat, qval, NULL);
        MemFree(qval);
    }

    if (feat.IsSetComment() && feat.GetComment().empty())
    {
        feat.ResetComment();
    }

    remove = 0;
    CRef<objects::CTrna_ext> trnac;
    if (feat.IsSetComment())
    {
        trnac = fta_get_trna_from_product(feat, feat.GetComment().c_str(), &remove);

        if (get_aa_from_trna(*trnac) == 0)
        {
            trnac = fta_get_trna_from_comment(feat.GetComment().c_str(), &remove);
        }

        if (get_aa_from_trna(*trnac) == 0 && get_first_codon_from_trna(*trnac) == 255)
        {
            trnac.Reset();
        }
    }

    if (trnaa.Empty())
    {
        if (trnap.Empty())
        {
            if (trnac.NotEmpty() && get_aa_from_trna(*trnac) != 0)
            {
                rna_ref->SetExt().SetTRNA(*trnac);
                if(remove != 0)
                {
                    feat.ResetComment();
                }
            }
        }
        else
        {
            rna_ref->SetExt().SetTRNA(*trnap);

            if (get_aa_from_trna(*trnap) == 0)
            {
                if (trnac.NotEmpty() && get_aa_from_trna(*trnac) != 0)
                    rna_ref->SetExt().SetTRNA(*trnac);
            }
            else if (trnac.NotEmpty())
            {
                if (get_aa_from_trna(*trnac) == 0 && get_first_codon_from_trna(*trnac) != 255 &&
                    get_first_codon_from_trna(*trnap) == 255 && remove != 0)
                {
                    trnap->SetCodon().assign(trnac->GetCodon().begin(), trnac->GetCodon().end());

                    feat.ResetComment();
                    if(remove == 2)
                        feat.SetComment("putative");
                }

                if (get_aa_from_trna(*trnac) == get_aa_from_trna(*trnap) && remove != 0)
                {
                    feat.ResetComment();
                }
            }
        }
    }
    else
    {
        if(trnap.NotEmpty())
        {
            trnap.Reset();
        }

        if (trnac.NotEmpty() && get_aa_from_trna(*trnac) != 0)
        {
            if (get_aa_from_trna(*trnac) == get_aa_from_trna(*trnaa) || get_aa_from_trna(*trnaa) == 88)
            {
                trnac->SetAnticodon(trnaa->SetAnticodon());
                trnaa->ResetAnticodon();

                if (get_first_codon_from_trna(*trnac) == 255)
                {
                    trnac->SetCodon().assign(trnaa->GetCodon().begin(), trnaa->GetCodon().end());
                }

                rna_ref->SetExt().SetTRNA(*trnac);
                if(remove != 0)
                {
                    feat.ResetComment();
                }
            }
        }
    }

    if (feat.GetQual().empty())
        feat.ResetQual();

    if (rna_ref->IsSetExt() && rna_ref->GetExt().IsTRNA())
    {
        const objects::CTrna_ext& trna = rna_ref->GetExt().GetTRNA();
        if (get_aa_from_trna(trna) == 0 && !trna.IsSetAnticodon())
        {
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
static void GetImpFeat(objects::CSeq_feat& feat, FeatBlkPtr fbp, bool locmap)
{
    CRef<objects::CImp_feat> imp_feat(new objects::CImp_feat);
    imp_feat->SetKey(fbp->key);

    if (locmap)
        imp_feat->SetLoc(fbp->location);

    feat.SetData().SetImp(*imp_feat);
}

/**********************************************************/
void fta_sort_biosource(objects::CBioSource& bio)
{
    if(bio.CanGetOrg() && !bio.GetOrg().GetDb().empty())
    {
        NON_CONST_ITERATE(objects::COrg_ref::TDb, db, bio.SetOrg().SetDb())
        {
            if (!(*db)->CanGetDb())
                continue;

            objects::COrg_ref::TDb::iterator tdb = db;
            for (++tdb; tdb != bio.SetOrg().SetDb().end(); ++tdb)
            {
                if (!(*tdb)->IsSetDb())
                    continue;

                if ((*db)->GetDb() < (*tdb)->GetDb())
                    continue;

                if ((*db)->GetDb() == (*tdb)->GetDb())
                {
                    const objects::CObject_id& db_id = (*db)->GetTag();
                    const objects::CObject_id& tdb_id = (*tdb)->GetTag();

                    if (!db_id.IsStr() && tdb_id.IsStr())
                        continue;

                    if (db_id.IsStr() && tdb_id.IsStr() &&
                        db_id.GetStr() <= tdb_id.GetStr())
                        continue;

                    if (!db_id.IsStr() && !tdb_id.IsStr() &&
                        db_id.GetId() <= tdb_id.GetId())
                        continue;
                }

                db->Swap(*tdb);
            }
        }
        
        if (bio.GetOrg().IsSetOrgname() && bio.GetOrg().GetOrgname().IsSetMod())
        {
            NON_CONST_ITERATE(objects::COrgName::TMod, mod, bio.SetOrg().SetOrgname().SetMod())
            {
                objects::COrgName::TMod::iterator tmod = mod;
                for (++tmod; tmod != bio.SetOrg().SetOrgname().SetMod().end(); ++tmod)
                {
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

    if (!bio.IsSetSubtype())
        return;

    NON_CONST_ITERATE(objects::CBioSource::TSubtype, sub, bio.SetSubtype())
    {
        objects::CBioSource::TSubtype::iterator tsub = sub;
        for (++tsub; tsub != bio.SetSubtype().end(); ++tsub)
        {
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
static void ConvertQualifierValue(CRef<objects::CGb_qual>& qual)
{
    std::string val = qual->GetVal();
    bool has_comma = val.find(',') != std::string::npos;

    if (has_comma)
    {
        std::replace(val.begin(), val.end(), ',', ';');
        qual->SetVal(val);
    }

    if (has_comma)
        ErrPostEx(SEV_WARNING, ERR_QUALIFIER_MultRptUnitComma,
        "Converting commas to semi-colons due to format conventions for multiple /rpt_unit qualifiers.");
}

/**********************************************************/
static void fta_parse_rpt_units(FeatBlkPtr fbp)
{
    char*   p;

    if(fbp == NULL || fbp->quals.empty())
        return;

    TQualVector::iterator first = fbp->quals.end();
    size_t len = 0, count = 0;

    for (TQualVector::iterator qual = fbp->quals.begin(); qual != fbp->quals.end();)
    {
        if ((*qual)->GetQual() != "rpt_unit")
        {
            ++qual;
            continue;
        }

        ErrPostEx(SEV_ERROR, ERR_QUALIFIER_ObsoleteRptUnit,
                  "Obsolete /rpt_unit qualifier found on feature \"%s\" at location \"%s\".",
                  (fbp->key == NULL) ? "Unknown" : fbp->key,
                  (fbp->location == NULL) ? "unknown" : fbp->location);

        if ((*qual)->GetVal().empty())
        {
            qual = fbp->quals.erase(qual);
            continue;
        }

        count++;
        len += (*qual)->GetVal().size();
        if (first == fbp->quals.end())
            first = qual;

        if (count == 1)
        {
            ++qual;
            continue;
        }

        if(count == 2)
            ConvertQualifierValue(*first);

        ConvertQualifierValue(*qual);
        ++qual;
    }

    if(count == 0)
        return;

    if(count == 1)
    {
        const std::string& val = (*first)->GetVal();
        if(*val.begin() == '(' && *val.rbegin() == ')')
        {
            ConvertQualifierValue(*first);
        }
        return;
    }

    p = (char*) MemNew(len + count + 2);
    StringCpy(p, "(");
    StringCat(p, (*first)->GetVal().c_str());

    for (TQualVector::iterator qual = first; qual != fbp->quals.end();)
    {
        if ((*qual)->GetQual() != "rpt_unit")
        {
            ++qual;
            continue;
        }

        StringCat(p, ",");
        StringCat(p, (*qual)->GetVal().c_str());
        qual = fbp->quals.erase(qual);
    }
    StringCat(p, ")");
    (*first)->SetVal(p);
}

/**********************************************************/
static bool fta_check_evidence(objects::CSeq_feat& feat, FeatBlkPtr fbp)
{
    Int4      evi_exp;
    Int4      evi_not;
    Int4      exp_good;
    Int4      exp_bad;
    Int4      inf_good;
    Int4      inf_bad;
    Char      ch;

    if (fbp == NULL || fbp->quals.empty())
        return true;

    evi_exp = 0;
    evi_not = 0;
    exp_good = 0;
    exp_bad = 0;
    inf_good = 0;
    inf_bad = 0;

    for (TQualVector::iterator qual = fbp->quals.begin(); qual != fbp->quals.end();)
    {
        const std::string& qual_str = (*qual)->IsSetQual() ? (*qual)->GetQual() : "";
        const std::string& val_str = (*qual)->IsSetVal() ? (*qual)->GetVal() : "";
        if (qual_str == "experiment")
        {
            if (val_str == "experimental evidence, no additional details recorded")
            {
                exp_good++;
                qual = fbp->quals.erase(qual);
            }
            else
            {
                exp_bad++;
                ++qual;
            }
            continue;
        }

        if (qual_str == "inference")
        {
            if (val_str == "non-experimental evidence, no additional details recorded")
            {
                inf_good++;
                qual = fbp->quals.erase(qual);
            }
            else
            {
                inf_bad++;
                ++qual;
            }
            continue;
        }

        if (qual_str != "evidence")
        {
            ++qual;
            continue;
        }

        if (NStr::CompareNocase(val_str.c_str(), "not_experimental") == 0)
            evi_not++;
        else if (NStr::CompareNocase(val_str.c_str(), "experimental") == 0)
            evi_exp++;
        else
        {
            if(fbp->location != NULL && StringLen(fbp->location) > 50)
            {
                ch = fbp->location[50];
                fbp->location[50] = '\0';
            }
            else
                ch = '\0';
            ErrPostEx(SEV_ERROR, ERR_QUALIFIER_InvalidEvidence,
                      "Illegal value \"%s\" for /evidence qualifier on the \"%s\" feature at \"%s\". Qualifier dropped.",
                      (val_str.empty()) ? "Unknown" : val_str.c_str(),
                      (fbp->key == NULL) ? "Unknown" : fbp->key,
                      (fbp->location == NULL) ? "unknown location" : fbp->location);
            if(ch != '\0')
                fbp->location[50] = ch;
        }

        qual = fbp->quals.erase(qual);
    }

    if(evi_exp + evi_not > 0 && exp_good + exp_bad + inf_good + inf_bad > 0)
    {
        if(fbp->location != NULL && StringLen(fbp->location) > 50)
        {
            ch = fbp->location[50];
            fbp->location[50] = '\0';
        }
        else
            ch = '\0';
        ErrPostEx(SEV_REJECT, ERR_QUALIFIER_Conflict,
                  "Old /evidence and new /experiment or /inference qualifiers both exist on the \"%s\" feature at \"%s\". This is currently unsupported.",
                  (fbp->key == NULL) ? "Unknown" : fbp->key,
                  (fbp->location == NULL) ? "unknown location" : fbp->location);
        if(ch != '\0')
            fbp->location[50] = ch;
        return false;
    }

    if(evi_exp + exp_good > 0 && evi_not + inf_good > 0)
    {
        if(fbp->location != NULL && StringLen(fbp->location) > 50)
        {
            ch = fbp->location[50];
            fbp->location[50] = '\0';
        }
        else
            ch = '\0';
        ErrPostEx(SEV_REJECT, ERR_QUALIFIER_Conflict,
                  "The special \"no additional details recorded\" values for both /experiment and /inference exist on the \"%s\" feature at \"%s\". This is currently unsupported.",
                  (fbp->key == NULL) ? "Unknown" : fbp->key,
                  (fbp->location == NULL) ? "unknown location" : fbp->location);
        if(ch != '\0')
            fbp->location[50] = ch;
        return false;
    }

    if((exp_good > 0 && exp_bad > 0) || (inf_good > 0 && inf_bad > 0))
    {
        if(fbp->location != NULL && StringLen(fbp->location) > 50)
        {
            ch = fbp->location[50];
            fbp->location[50] = '\0';
        }
        else
            ch = '\0';
        ErrPostEx(SEV_REJECT, ERR_QUALIFIER_Conflict,
                  "The special \"no additional details recorded\" value for /experiment or /inference exists in conjunction with other /experiment or /inference qualifiers on the \"%s\" feature at \"%s\". This is currently unsupported.",
                  (fbp->key == NULL) ? "Unknown" : fbp->key,
                  (fbp->location == NULL) ? "unknown location" : fbp->location);
        if(ch != '\0')
            fbp->location[50] = ch;
        return false;
    }

    if(exp_good + evi_exp > 0)
        feat.SetExp_ev(objects::CSeq_feat::eExp_ev_experimental);
    else if (inf_good + evi_not > 0)
        feat.SetExp_ev(objects::CSeq_feat::eExp_ev_not_experimental);
    return true;
}

/**********************************************************
 *
 *   static CRef<objects::CSeq_feat> ProcFeatBlk(pp, fbp, seqids):
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
static CRef<objects::CSeq_feat> ProcFeatBlk(ParserPtr pp, FeatBlkPtr fbp, TSeqIdList& seqids)
{
    const char **b;

    char* loc = NULL;

    bool    locmap = false;
    bool    err = false;

    CRef<objects::CSeq_feat> feat;

    if (fbp->location != NULL)
    {
        loc = fbp->location;
        DelCharBtwData(loc);
        if(pp->buf != NULL)
            MemFree(pp->buf);
        pp->buf = (char*) MemNew(StringLen(fbp->key) + StringLen(loc) + 4);
        StringCpy(pp->buf, fbp->key);
        StringCat(pp->buf, " : ");
        StringCat(pp->buf, loc);

        feat.Reset(new objects::CSeq_feat);
        locmap = GetSeqLocation(*feat, loc, seqids, &err, pp, fbp->key);

        if(pp->buf != NULL)
            MemFree(pp->buf);
        pp->buf = NULL;
    }
    if(err)
    {
        if(pp->debug == false)
        {
            ErrPostEx(SEV_ERROR, ERR_FEATURE_Dropped,
                      "%s|%s| range check detects problems", fbp->key, loc);
            feat.Reset();
            return feat;
        }
        ErrPostEx(SEV_WARNING, ERR_LOCATION_FailedCheck,
                  "%s|%s| range check detects problems", fbp->key, loc);
    }

    if (!fbp->quals.empty()) {
        if (DeleteQual(fbp->quals, "partial"))
            feat->SetPartial(true);
    }

    if (StringStr(loc, "order") != NULL)
        feat->SetPartial(true);

    if (!fbp->quals.empty())
    {
        if (DeleteQual(fbp->quals, "pseudo"))
            feat->SetPseudo(true);
    }

    if (!fbp->quals.empty())
        DeleteQual(fbp->quals, "gsdb_id");

    if (!fbp->quals.empty())
        fta_parse_rpt_units(fbp);

    if (!fbp->quals.empty())
    {
        for(b = TransSplicingFeats; *b != NULL; b++)
            if(StringCmp(fbp->key, *b) == 0)
                break;
        if (*b != NULL && DeleteQual(fbp->quals, "trans_splicing"))
        {
            feat->SetExcept(true);
            if (!feat->IsSetExcept_text())
                feat->SetExcept_text("trans-splicing");
            else
            {
                std::string& exc_text = feat->SetExcept_text();
                exc_text += ", trans-splicing";
            }
        }
    }

    if(!fta_check_evidence(*feat, fbp))
    {
        pp->entrylist[pp->curindx]->drop = 1;
        return feat;
    }

    if ((!feat->IsSetPartial() || !feat->GetPartial()) && StringCmp(fbp->key, "gap") != 0) {
        if (SeqLocHaveFuzz(feat->GetLocation()))
            feat->SetPartial(true);
    }

    if (!fbp->quals.empty())
    {
        const Char* comment = GetTheQualValue(fbp->quals, "note");

        if (comment && comment[0])
            feat->SetComment(comment);
    }

    /* assume all imp for now
     */
    if (StringStr(fbp->key, "source") == NULL)
        GetImpFeat(*feat, fbp, locmap);

    ITERATE(TQualVector, cur, fbp->quals)
    {
        const std::string& qual_str = (*cur)->GetQual();
        if (qual_str == "pseudogene")
            feat->SetPseudo(true);

        // Do nothing for 'translation' qualifier in case of its value is empty
        if (qual_str == "translation" && (!(*cur)->IsSetVal() || (*cur)->GetVal().empty()))
            continue;

        if (!qual_str.empty())
            feat->SetQual().push_back(*cur);
    }

    return feat;
}

/**********************************************************/
static void fta_get_gcode_from_biosource(const objects::CBioSource& bio_src, IndexblkPtr ibp)
{
    if (!bio_src.IsSetOrg() || !bio_src.GetOrg().IsSetOrgname())
        return;

    ibp->gc_genomic = bio_src.GetOrg().GetOrgname().IsSetGcode() ? bio_src.GetOrg().GetOrgname().GetGcode() : 0;
    ibp->gc_mito = bio_src.GetOrg().GetOrgname().IsSetMgcode() ? bio_src.GetOrg().GetOrgname().GetMgcode() : 0;
}

/**********************************************************/
static void fta_sort_quals(FeatBlkPtr fbp, bool qamode)
{
    if(fbp == NULL)
        return;

    NON_CONST_ITERATE(TQualVector, q, fbp->quals)
    {
        if((*q)->GetQual() == "gene" ||
           (!qamode && (*q)->GetQual() == "product"))
            continue;

        TQualVector::iterator tq = q;
        for (++tq; tq != fbp->quals.end(); ++tq)
        {
            const std::string& q_qual = (*q)->GetQual();
            const std::string& tq_qual = (*tq)->GetQual();

            if (!tq_qual.empty())
            {
                if (q_qual == "gene")
                    continue;

                Int4 i = NStr::CompareNocase(q_qual.c_str(), tq_qual.c_str());
                if(i < 0)
                    continue;
                if(i == 0)
                {
                    /* Do not sort /gene qualifiers
                     */
                    const std::string q_val = (*q)->GetVal();
                    const std::string tq_val = (*tq)->GetVal();

                    if (q_val.empty())
                        continue;

                    if(!tq_val.empty())
                    {
                        if(q_val[0] >= '0' && q_val[0] <= '9' &&
                           tq_val[0] >= '0' && tq_val[0] <= '9')
                        {
                            if(atoi(q_val.c_str()) <= atoi(tq_val.c_str()))
                                continue;
                        }
                        else if(q_val <= tq_val)
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

    ITERATE(TQualVector, gbqp1, qual1)
    {
        found = false;
        ITERATE(TQualVector, gbqp2, qual2)
        {
            const Char* qual_a = (*gbqp1)->IsSetQual() ? (*gbqp1)->GetQual().c_str() : NULL;
            const Char* qual_b = (*gbqp2)->IsSetQual() ? (*gbqp2)->GetQual().c_str() : NULL;

            const Char* val_a = (*gbqp1)->IsSetVal() ? (*gbqp1)->GetVal().c_str() : NULL;
            const Char* val_b = (*gbqp2)->IsSetVal() ? (*gbqp2)->GetVal().c_str() : NULL;

            if (fta_strings_same(qual_a, qual_b) && fta_strings_same(val_a, val_b))
            {
                found = true;
                break;
            }
        }
        if (!found)
            break;
    }

    if (!found)
        return false;

    return true;
}

/**********************************************************/
static bool fta_feats_same(FeatBlkPtr fbp1, FeatBlkPtr fbp2)
{
    if(fbp1 == NULL && fbp2 == NULL)
        return true;
    if(fbp1 == NULL || fbp2 == NULL ||
       fta_strings_same(fbp1->key, fbp2->key) == false ||
       fta_strings_same(fbp1->location, fbp2->location) == false)
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
    Int4    i1;
    Int4    i2;

    if(val == NULL || *val == '\0')
        return false;

    for(p = val; *p >= '0' && *p <= '9';)
        p++;

    if(p == val || p[0] != '.' || p[1] != '.')
        return false;

    i1 = atoi(val);
    for(p += 2, q = p; *q >= '0' && *q <= '9';)
        q++;
    if(q == p || *q != '\0')
        return false;
    i2 = atoi(p);

    if(i1 == 0 || i1 > i2 || i2 > (Int4) length)
        return false;
    return true;
}

/**********************************************************/
static void fta_check_rpt_unit_range(FeatBlkPtr fbp, size_t length)
{
    Char      ch;

    if (fbp == NULL || fbp->quals.empty())
        return;

    for (TQualVector::iterator cur = fbp->quals.begin(); cur != fbp->quals.end();)
    {
        if (!(*cur)->IsSetQual() || !(*cur)->IsSetVal())
        {
            ++cur;
            continue;
        }

        const std::string& qual_str = (*cur)->GetQual();
        const std::string& val_str = (*cur)->GetVal();

        if (qual_str != "rpt_unit_range" || fta_check_rpt_unit_span(val_str.c_str(), length))
        {
            ++cur;
            continue;
        }

        if(fbp->location != NULL && StringLen(fbp->location) > 20)
        {
            ch = fbp->location[20];
            fbp->location[20] = '\0';
        }
        else
            ch = '\0';
        ErrPostEx(SEV_ERROR, ERR_QUALIFIER_InvalidRptUnitRange,
                  "/rpt_unit_range qualifier \"%s\" on feature \"%s\" at location \"%s%s\" is not a valid basepair range. Qualifier dropped.",
                  val_str.empty() ? "(EMPTY)" : val_str.c_str(),
                  (fbp->key == NULL) ? "Unknown" : fbp->key,
                  (fbp->location == NULL) ? "unknown" : fbp->location,
                  (ch == '\0') ? "" : "...");
        if(ch != '\0')
            fbp->location[20] = ch;

        cur = fbp->quals.erase(cur);
    }
}

/**********************************************************/
static void fta_remove_dup_feats(DataBlkPtr dbp)
{
    DataBlkPtr tdbp;
    DataBlkPtr tdbpprev;
    DataBlkPtr tdbpnext;
    FeatBlkPtr fbp1;
    FeatBlkPtr fbp2;
    Char       ch;

    if(dbp == NULL || dbp->mpNext == NULL)
        return;

    for(; dbp != NULL; dbp = dbp->mpNext)
    {
        if(dbp->mpData == NULL)
            continue;

        fbp1 = (FeatBlkPtr) dbp->mpData;
        tdbpprev = dbp;
        for(tdbp = dbp->mpNext; tdbp != NULL; tdbp = tdbpnext)
        {
            tdbpnext = tdbp->mpNext;
            if(tdbp->mpData == NULL)
            {
                tdbpprev->mpNext = tdbpnext;
                MemFree(tdbp);
                continue;
            }

            fbp2 = (FeatBlkPtr) tdbp->mpData;

            if(fbp1->location != NULL && fbp2->location != NULL &&
               StringCmp(fbp1->location, fbp2->location) < 0)
                break;

            if(!fta_feats_same(fbp1, fbp2))
            {
                tdbpprev = tdbp;
                continue;
            }

            if(fbp2->location != NULL && StringLen(fbp2->location) > 20)
            {
                ch = fbp2->location[20];
                fbp2->location[20] = '\0';
            }
            else
                ch = '\0';
            ErrPostEx(SEV_WARNING, ERR_FEATURE_DuplicateRemoved,
                      "Duplicated feature \"%s\" at location \"%s%s\" removed.",
                      (fbp2->key == NULL) ? "???" : fbp2->key,
                      (fbp2->location == NULL) ? "???" : fbp2->location,
                      (ch == '\0') ? "" : "...");

            FreeFeatBlkQual(fbp2);
            tdbpprev->mpNext = tdbpnext;
            MemFree(tdbp);
        }
    }
}

/**********************************************************/
class PredIsGivenQual
{
public:
    PredIsGivenQual(const std::string& qual) : qual_(qual) {}

    bool operator()(const CRef<objects::CGb_qual>& qual)
    {
        return qual->GetQual() == qual_;
    }

private:
    std::string qual_;
};

static void fta_check_multiple_locus_tag(DataBlkPtr dbp, unsigned char* drop)
{
    FeatBlkPtr fbp;
    Char       ch;

    for(; dbp != NULL; dbp = dbp->mpNext)
    {
        fbp = (FeatBlkPtr) dbp->mpData;
        if(fbp == NULL)
            continue;

        size_t i = std::count_if(fbp->quals.begin(), fbp->quals.end(), PredIsGivenQual("locus_tag"));
        if(i < 2)
            continue;

        if(fbp->location != NULL && StringLen(fbp->location) > 50)
        {
            ch = fbp->location[50];
            fbp->location[50] = '\0';
        }
        else
            ch = '\0';
        ErrPostEx(SEV_REJECT, ERR_FEATURE_MultipleLocusTags,
                  "Multiple /locus_tag values for \"%s\" feature at \"%s\".",
                  (fbp->key == NULL) ? "Unknown" : fbp->key,
                  (fbp->location == NULL) ? "unknown location" : fbp->location);
        if(ch != '\0')
            fbp->location[50] = ch;
        *drop = 1;
        break;
    }
}

/**********************************************************/
static void fta_check_old_locus_tags(DataBlkPtr dbp, unsigned char* drop)
{
    Int4       i;

    PredIsGivenQual isOldLocusTag("old_locus_tag"),
                    isLocusTag("locus_tag");

    for(; dbp != NULL; dbp = dbp->mpNext)
    {
        FeatBlkPtr fbp = (FeatBlkPtr)dbp->mpData;
        if(fbp == NULL)
            continue;
        size_t olt = std::count_if(fbp->quals.begin(), fbp->quals.end(), isOldLocusTag);
        size_t lt = std::count_if(fbp->quals.begin(), fbp->quals.end(), isLocusTag);

        if(olt == 0)
            continue;

        if(lt == 0)
        {
            ErrPostEx(SEV_REJECT, ERR_FEATURE_OldLocusTagWithoutNew,
                      "Feature \"%s\" at \"%s\" has an /old_locus_tag qualifier but lacks a /locus_tag qualifier. Entry dropped.",
                      (fbp->key == NULL) ? "Unknown" : fbp->key,
                      (fbp->location == NULL) ? "unknown location" : fbp->location);
            *drop = 1;
        }
        else
        {
            i = 0;
            ITERATE(TQualVector, gbqp1, fbp->quals)
            {
                if (!(*gbqp1)->IsSetQual() || !(*gbqp1)->IsSetVal() || !isLocusTag(*gbqp1))
                    continue;

                i++;

                const std::string& gbqp1_val = (*gbqp1)->GetVal();
                if (gbqp1_val.empty())
                    continue;

                ITERATE(TQualVector, gbqp2, fbp->quals)
                {
                    if (!(*gbqp2)->IsSetQual() || !(*gbqp2)->IsSetVal())
                        continue;

                    const std::string& gbqp2_val = (*gbqp2)->GetVal();

                    if (!isOldLocusTag(*gbqp2) || !NStr::EqualNocase(gbqp1_val, gbqp2_val))
                        continue;

                    ErrPostEx(SEV_REJECT, ERR_FEATURE_MatchingOldNewLocusTag,
                              "Feature \"%s\" at \"%s\" has an /old_locus_tag qualifier with a value that is identical to that of a /locus_tag qualifier: \"%s\". Entry dropped.",
                              (fbp->key == NULL) ? "Unknown" : fbp->key,
                              (fbp->location == NULL) ? "unknown location" : fbp->location,
                              gbqp1_val.c_str());
                    *drop = 1;
                }
            }
        }

        if(olt == 1)
            continue;

        ITERATE(TQualVector, gbqp1, fbp->quals)
        {
            const std::string& gbqp1_val = (*gbqp1)->GetVal();
            if (isOldLocusTag(*gbqp1) || gbqp1_val.empty())
                continue;

            TQualVector::const_iterator gbqp2 = gbqp1;
            for (++gbqp2; gbqp2 != fbp->quals.end(); ++gbqp2)
            {
                const std::string& gbqp2_val = (*gbqp2)->GetVal();
                if (isOldLocusTag(*gbqp2) || gbqp2_val.empty())
                    continue;

                if (NStr::CompareNocase(gbqp1_val.c_str(), gbqp2_val.c_str()) == 0)
                {
                    ErrPostEx(SEV_ERROR, ERR_FEATURE_RedundantOldLocusTag,
                              "Feature \"%s\" at \"%s\" has redundant /old_locus_tag qualifiers. Dropping all but the first.",
                              (fbp->key == NULL) ? "Unknown" : fbp->key,
                              (fbp->location == NULL) ? "unknown location" : fbp->location);
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
    bool    got_pseudogene;
    bool    got_pseudo;

    for(; dbp != NULL; dbp = dbp->mpNext)
    {
        fbp = (FeatBlkPtr) dbp->mpData;
        if(fbp == NULL)
            continue;

        got_pseudo = false;
        got_pseudogene = false;

        for (TQualVector::iterator cur = fbp->quals.begin(); cur != fbp->quals.end(); )
        {
            const std::string& qual_str = (*cur)->GetQual();
            const std::string& val_str = (*cur)->IsSetVal() ? (*cur)->GetVal() : "";

            if (qual_str != "pseudogene")
            {
                if(!got_pseudo && qual_str == "pseudo")
                    got_pseudo = true;
                ++cur;
                continue;
            }

            if(got_pseudogene)
            {
                ErrPostEx(SEV_ERROR, ERR_QUALIFIER_MultiplePseudoGeneQuals,
                          "Dropping a /pseudogene qualifier because multiple /pseudogene qualifiers are present : <%s> : Feature key <%s> : Feature location <%s>.",
                          val_str.empty() ? "[empty]" : val_str.c_str(),
                          fbp->key, fbp->location);

                cur = fbp->quals.erase(cur);
                continue;
            }

            got_pseudogene = true;

            if (val_str.empty())
            {
                ErrPostEx(SEV_ERROR, ERR_QUALIFIER_InvalidPseudoGeneValue,
                          "Dropping a /pseudogene qualifier because its value is empty : Feature key <%s> : Feature location <%s>.",
                          fbp->key, fbp->location);

                cur = fbp->quals.erase(cur);
                continue;
            }

            if(MatchArrayString(PseudoGeneValues, val_str.c_str()) >= 0)
            {
                ++cur;
                continue;
            }

            ErrPostEx(SEV_ERROR, ERR_QUALIFIER_InvalidPseudoGeneValue,
                      "Dropping a /pseudogene qualifier because its value is invalid : <%s> : Feature key <%s> : Feature location <%s>.",
                      val_str.c_str(), fbp->key, fbp->location);

            cur = fbp->quals.erase(cur);
        }

        if(!got_pseudogene || !got_pseudo)
            continue;

        ErrPostEx(SEV_ERROR, ERR_QUALIFIER_OldPseudoWithPseudoGene,
                  "A legacy /pseudo qualifier and a /pseudogene qualifier are present on the same feature : Dropping /pseudo : Feature key <%s> : Feature location <%s>.",
                  fbp->key, fbp->location);
        DeleteQual(fbp->quals, "pseudo");
    }
}

/**********************************************************/
static void fta_check_compare_qual(DataBlkPtr dbp, bool is_tpa)
{
    FeatBlkPtr fbp;
    char*    p;
    char*    q;
    bool       badcom;
    Char       ch;
    Int4       com_count;
    Int4       cit_count;

    for(; dbp != NULL; dbp = dbp->mpNext)
    {
        fbp = (FeatBlkPtr) dbp->mpData;
        if(fbp == NULL)
            continue;

        com_count = 0;
        cit_count = 0;

        for (TQualVector::iterator cur = fbp->quals.begin(); cur != fbp->quals.end();)
        {
            const std::string& qual_str = (*cur)->GetQual();
            const std::string& val_str = (*cur)->IsSetVal() ? (*cur)->GetVal() : "";

            if (qual_str == "compare")
            {
                badcom = true;
                if (!val_str.empty())
                {
                    q = StringChr(val_str.c_str(), '.');
                    if(q != NULL && q[1] != '\0')
                    {
                        for(p = q + 1; *p >= '0' && *p <= '9';)
                            p++;
                        if(*p == '\0')
                        {
                            *q = '\0';
                            if (GetNucAccOwner(val_str.c_str(), is_tpa) > 0)
                                badcom = false;
                            *q = '.';
                        }
                    }
                }

                if(badcom)
                {
                    ErrPostEx(SEV_ERROR, ERR_QUALIFIER_IllegalCompareQualifier,
                              "/compare qualifier value is not a legal Accession.Version : feature \"%s\" at \"%s\" : value \"%s\" : qualifier has been dropped.",
                              fbp->key, fbp->location,
                              val_str.empty() ? "[empty]" : val_str.c_str());

                    cur = fbp->quals.erase(cur);
                    continue;
                }
                com_count++;
            }
            else if (qual_str == "citation")
                cit_count++;

            ++cur;
        }

        if(com_count > 0 || cit_count > 0 ||
           (StringCmp(fbp->key, "old_sequence") != 0 &&
            StringCmp(fbp->key, "conflict") != 0))
            continue;

        ch = '\0';
        if(StringLen(fbp->location) > 30)
        {
            ch = fbp->location[30];
            fbp->location[30] = '\0';
        }
        ErrPostEx(SEV_ERROR, ERR_FEATURE_RequiredQualifierMissing,
                  "Feature \"%s\" at \"%s\" lacks required /citation and/or /compare qualifier : feature has been dropped.",
                  fbp->key, fbp->location);
        if(ch != '\0')
            fbp->location[30] = ch;
        dbp->mDrop = true;
    }
}

/**********************************************************/
static void fta_check_non_tpa_tsa_tls_locations(DataBlkPtr dbp,
                                                IndexblkPtr ibp)
{
    FeatBlkPtr fbp;
    char*    location;
    char*    p;
    char*    q;
    char*    r;
    Uint1      i;

    location = NULL;
    for(; dbp != NULL; dbp = dbp->mpNext)
    {
        fbp = (FeatBlkPtr) dbp->mpData;
        if(fbp == NULL || fbp->location == NULL)
            continue;
        location = StringSave(fbp->location);
        for(p = location, q = p; *p != '\0'; p++)
            if(*p != ' ' && *p != '\t' && *p != '\n')
                *q++ = *p;
        *q = '\0';
        if(q == location)
        {
            MemFree(location);
            location = NULL;
            continue;
        }

        for(p = location + 1; *p != '\0'; p++)
        {
            if(*p != ':')
                continue;
            for(r = NULL, q = p - 1;; q--)
            {
                if(q == location)
                {
                    if(*q != '_' && (*q < '0' || *q > '9') &&
                       (*q < 'a' || *q > 'z') && (*q < 'A' || *q > 'Z'))
                        q++;
                    break;
                }
                if(*q == '.')
                {
                    if(r == NULL)
                    {
                        r = q;
                        continue;
                    }
                    q++;
                    break;
                }
                if(*q != '_' && (*q < '0' || *q > '9') &&
                   (*q < 'a' || *q > 'z') && (*q < 'A' || *q > 'Z'))
                {
                    q++;
                    break;
                }
            }
            if(q == p)
                continue;
            if(r != NULL)
                *r = '\0';
            else
                *p = '\0';
            i = GetNucAccOwner(q, ibp->is_tpa);
            if(r != NULL)
                *r = '.';
            else
                *p = ':';


            if (i == objects::CSeq_id::e_Genbank && (q[0] == 'e' || q[0] == 'E') &&
               (q[1] == 'z' || q[1] == 'Z') && ibp->is_tpa == false)
                continue;
            if (ibp->is_tpa && (i == objects::CSeq_id::e_Tpg || i == objects::CSeq_id::e_Tpd ||
                i == objects::CSeq_id::e_Tpe))
                continue;
            break;
        }
        if(*p != '\0')
            break;
        if(location != NULL)
        {
            MemFree(location);
            location = NULL;
        }
    }
    if(dbp == NULL)
        return;

    ibp->drop = 1;
    if(location != NULL && StringLen(location) > 45)
    {
        location[40] = '\0';
        StringCat(location, "...");
    }
    if(ibp->is_tsa)
        ErrPostEx(SEV_REJECT, ERR_LOCATION_AccessionNotTSA,
                  "Feature \"%s\" at \"%s\" on a TSA record cannot point to a non-TSA record.",
                  fbp->key, (location == NULL) ? "empty_location" : location);
    else if(ibp->is_tls)
        ErrPostEx(SEV_REJECT, ERR_LOCATION_AccessionNotTLS,
                  "Feature \"%s\" at \"%s\" on a TLS record cannot point to a non-TLS record.",
                  fbp->key, (location == NULL) ? "empty_location" : location);
    else
        ErrPostEx(SEV_REJECT, ERR_LOCATION_AccessionNotTPA,
                  "Feature \"%s\" at \"%s\" on a TPA record cannot point to a non-TPA record.",
                  fbp->key, (location == NULL) ? "empty_location" : location);
    if(location != NULL)
        MemFree(location);
}

/**********************************************************/
static bool fta_perform_operon_checks(TSeqFeatList& feats, IndexblkPtr ibp)
{
    FTAOperonPtr fophead(nullptr);
    FTAOperonPtr fop;
    FTAOperonPtr tfop;

    //using FTAOperonList = list<FTAOperonPtr>;
    //FTAOperonList operonList;

    bool got(false);
    int count(0);

    if (feats.empty()) {
        return true;
    }

    fophead = nullptr;
    ITERATE(TSeqFeatList, feat, feats)
    {
        if (!(*feat)->GetData().IsImp())
            continue;

        const objects::CImp_feat& imp_feat = (*feat)->GetData().GetImp();

        count = 0;
        ITERATE(objects::CSeq_feat::TQual, qual, (*feat)->GetQual())
        {
            if (!(*qual)->IsSetQual() || (*qual)->GetQual() != "operon" ||
                !(*qual)->IsSetVal() || (*qual)->GetVal().empty())
                continue;

            tfop = new FTAOperon(
                imp_feat.IsSetKey() ? imp_feat.GetKey().c_str() : "Unknown",
                (*qual)->GetVal());
            tfop->location = &(*feat)->GetLocation();
            //operonList.push_back(tfop);

            fophead ? fop->next = tfop : fophead = tfop;
            fop = tfop;
            count++;

            if(fop->operon_feat == false || fop == fophead)
                continue;

            for(tfop = fophead; tfop->next != NULL; tfop = tfop->next)
            {
                if(tfop->operon_feat == false || tfop->mOperon != fop->mOperon)
                    continue;

                if(tfop->strloc == NULL)
                    tfop->strloc = location_to_string_or_unknown(*tfop->location);

                if(fop->strloc == NULL)
                    fop->strloc = location_to_string_or_unknown(*fop->location);

                ErrPostEx(SEV_REJECT, ERR_FEATURE_OperonQualsNotUnique,
                        "The operon features at \"%s\" and \"%s\" utilize the same /operon qualifier : \"%s\".",
                        tfop->strloc.c_str(), fop->strloc.c_str(), fop->mOperon.c_str());
                fophead->ret = false;
            }
        }

        if(count > 1)
        {
            if(fop->strloc == NULL)
                fop->strloc = location_to_string_or_unknown(*fop->location);

            ErrPostEx(SEV_REJECT, ERR_FEATURE_MultipleOperonQuals,
                    "Feature \"%s\" at \"%s\" has more than one operon qualifier.",
                    fop->mFeatname.c_str(), fop->strloc.c_str());
            fophead->ret = false;
        }

        if (count == 0 && imp_feat.IsSetKey() && imp_feat.GetKey() == "operon")
        {
            string loc = location_to_string_or_unknown((*feat)->GetLocation());

            ErrPostEx(SEV_REJECT, ERR_FEATURE_MissingOperonQual,
                "The operon feature at \"%s\" lacks an /operon qualifier.",
                loc.c_str());

            fophead->ret = false;
        }
    }

    if (!fophead  || (ibp->segnum != 0 && ibp->segnum != ibp->segtotal)) {
        return true;
    }

    if(fophead->next == NULL || fophead->next->next == NULL)
        return(fophead->ret);

    for(fop = fophead->next; fop != NULL; fop = fop->next)
    {
        if(fop->operon_feat)
            continue;

        got = false;
        for(tfop = fophead->next; tfop != NULL; tfop = tfop->next)
        {
            if(tfop->operon_feat == false ||  fop->mOperon != tfop->mOperon)
                continue;

            got = true;
            objects::sequence::ECompare cmp_res = objects::sequence::Compare(*fop->location, *tfop->location, nullptr, objects::sequence::fCompareOverlapping);
            if (cmp_res == objects::sequence::eContained || cmp_res == objects::sequence::eSame)
                continue;

            if(fop->strloc == NULL)
                fop->strloc = location_to_string_or_unknown(*fop->location);

            if(tfop->strloc == NULL)
                tfop->strloc = location_to_string_or_unknown(*tfop->location);

            ErrPostEx(SEV_REJECT, ERR_FEATURE_OperonLocationMisMatch,
                      "Feature \"%s\" at \"%s\" with /operon qualifier \"%s\" does not fall within the span of the operon feature at \"%s\".",
                      fop->mFeatname.c_str(), fop->strloc.c_str(), fop->mOperon.c_str(), tfop->strloc.c_str());
            fophead->ret = false;
        }

        if(!got) {
            if(fop->strloc == NULL)
                fop->strloc = location_to_string_or_unknown(*fop->location);

            ErrPostEx(SEV_REJECT, ERR_FEATURE_InvalidOperonQual,
                    "/operon qualifier \"%s\" on feature \"%s\" at \"%s\" has a value that does not match any of the /operon qualifiers on operon features.",
                    fop->mOperon.c_str(), fop->mFeatname.c_str(), fop->strloc.c_str());
            fophead->ret = false;
        }
    }

    got = fophead->ret;
    delete fophead;
    return got;
}

/**********************************************************/
static void fta_remove_dup_quals(FeatBlkPtr fbp)
{
    Char      ch;

    if(fbp == NULL || fbp->quals.empty())
        return;

    NON_CONST_ITERATE(TQualVector, cur, fbp->quals)
    {
        const char* cur_qual = (*cur)->IsSetQual() ? (*cur)->GetQual().c_str() : NULL;
        const char* cur_val = (*cur)->IsSetVal() ? (*cur)->GetVal().c_str() : NULL;

        TQualVector::iterator next = cur;
        for (++next; next != fbp->quals.end();)
        {
            const char* next_qual = (*next)->IsSetQual() ? (*next)->GetQual().c_str() : NULL;
            const char* next_val = (*next)->IsSetVal() ? (*next)->GetVal().c_str() : NULL;

            if (!fta_strings_same(cur_qual, next_qual) || !fta_strings_same(cur_val, next_val))
            {
                ++next;
                continue;
            }

            if(fbp->location != NULL && StringLen(fbp->location) > 20)
            {
                ch = fbp->location[20];
                fbp->location[20] = '\0';
            }
            else
                ch = '\0';

            ErrPostEx(SEV_ERROR, ERR_QUALIFIER_DuplicateRemoved,
                      "Duplicated qualifier \"%s\" in feature \"%s\" at location \"%s%s\" removed.",
                      (cur_qual == NULL) ? "???" : cur_qual,
                      (fbp->key == NULL) ? "???" : fbp->key,
                      (fbp->location == NULL) ? "???" : fbp->location,
                      (ch == '\0') ? "" : "...");

            if(ch != '\0')
                fbp->location[20] = ch;

            next = fbp->quals.erase(next);
        }
    }
}

/**********************************************************/
static void CollectGapFeats(const DataBlk& entry, DataBlkPtr dbp,
                            ParserPtr pp, Int2 type)
{
    IndexblkPtr        ibp;
    GapFeatsPtr        gfp = NULL;
    GapFeatsPtr        tgfp;
    DataBlkPtr         tdbp;
    FeatBlkPtr         fbp;

    objects::CLinkage_evidence::TLinkage_evidence asn_linkage_evidence;
    std::list<std::string> linkage_evidence_names;

    StrNumPtr          snp;
    char*            p;
    char*            q;
    const char*    gap_type;
    bool               finished_gap;
    ErrSev             sev;
    Int4               estimated_length;
    Int4               is_htg;
    Int4               from;
    Int4               to;
    Int4               prev_gap;        /* 0 - initial, 1 - "gap",
                                           2 - "assembly_gap" */
    Int4               curr_gap;        /* 0 - initial, 1 - "gap",
                                           2 - "assembly_gap" */
    Int4               asn_gap_type;

    ibp = pp->entrylist[pp->curindx];

    if(ibp->keywords.empty())
    {
        if(pp->format == Parser::EFormat::GenBank)
            GetSequenceOfKeywords(entry, ParFlat_KEYWORDS,
                                  ParFlat_COL_DATA, ibp->keywords);
        else if(pp->format == Parser::EFormat::EMBL)
            GetSequenceOfKeywords(entry, ParFlat_KW, ParFlat_COL_DATA_EMBL,
                                  ibp->keywords);
        else if(pp->format == Parser::EFormat::XML)
            XMLGetKeywords(entry.mOffset, ibp->xip, ibp->keywords);
    }

    is_htg = -1;
    ITERATE(TKeywordList, key, ibp->keywords)
    {
        if(is_htg >= 0 && is_htg <= 2)
            break;
        if(*key == "HTG")
            is_htg = 3;
        else if(*key == "HTGS_PHASE0")
            is_htg = 0;
        else if(*key == "HTGS_PHASE1")
            is_htg = 1;
        else if(*key == "HTGS_PHASE2")
            is_htg = 2;
        else if(*key == "HTGS_PHASE3")
            is_htg = 3;
    }

    prev_gap = 0;
    curr_gap = 0;
    finished_gap = false;
    for(ibp->gaps = NULL; dbp != NULL; dbp = dbp->mpNext)
    {
        if(ibp->drop != 0)
            break;
        if(dbp->mType != type)
            continue;

        linkage_evidence_names.clear();
        asn_linkage_evidence.clear();

        for(tdbp = (DataBlkPtr) dbp->mpData; tdbp != NULL; tdbp = tdbp->mpNext)
        {
            if(ibp->drop != 0)
                break;
            fbp = (FeatBlkPtr) tdbp->mpData;
            if(fbp == NULL || fbp->key == NULL)
                continue;
            if(StringCmp(fbp->key, "gap") == 0)
            {
                prev_gap = curr_gap;
                curr_gap = 1;
            }
            else if(StringCmp(fbp->key, "assembly_gap") == 0)
            {
                prev_gap = curr_gap;
                curr_gap = 2;
            }
            else
                continue;

            from = 0;
            to = 0;
            estimated_length = 0;
            gap_type = NULL;
            linkage_evidence_names.clear();
            asn_gap_type = -1;
            asn_linkage_evidence.clear();
            estimated_length = -1;

            ITERATE(TQualVector, cur, fbp->quals)
            {
                if (!(*cur)->IsSetQual() || !(*cur)->IsSetVal())
                    continue;

                const std::string& cur_qual = (*cur)->GetQual();
                const std::string& cur_val = (*cur)->GetVal();

                if (cur_qual.empty() || cur_val.empty())
                    continue;

                if (cur_qual == "estimated_length")
                {
                    if (cur_val == "unknown")
                        estimated_length = -100;
                    else
                    {
                        const char* cp = cur_val.c_str();
                        for (; *cp >= '0' && *cp <= '9';)
                            ++cp;
                        if(*cp == '\0')
                            estimated_length = atoi(cur_val.c_str());
                    }
                }
                else if (cur_qual == "gap_type")
                    gap_type = cur_val.c_str();
                else if (cur_qual == "linkage_evidence")
                {
                    linkage_evidence_names.push_back(cur_val);
                }
            }

            if(fbp->location != NULL)
            {
                p = fbp->location;
                if(*p == '<')
                    p++;
                for(q = p; *p >= '0' && *p <= '9';)
                    p++;
                if(*p == '\0')
                {
                    from = atoi(q);
                    to = from;
                }
                else if(*p == '.')
                {
                    *p = '\0';
                    from = atoi(q);
                    *p++ = '.';
                    if(*fbp->location == '<' && from != 1)
                        from = 0;
                    else if(*p == '.')
                    {
                        if(*++p == '>')
                           p++;
                        for(q = p; *p >= '0' && *p <= '9';)
                            p++;
                        if(*p == '\0')
                            to = atoi(q);
                        if(*(q - 1) == '>' && to != (int) ibp->bases)
                            to = 0;
                    }
                }
            }

            if(from == 0 || to == 0 || from > to)
            {
                if(curr_gap == 1)
                    ErrPostEx(SEV_REJECT, ERR_FEATURE_InvalidGapLocation,
                              "Invalid gap feature location : \"%s\" : all gap features must have a simple X..Y location on the plus strand.",
                              (fbp->location == NULL) ? "unknown" : fbp->location);
                else
                    ErrPostEx(SEV_REJECT, ERR_FEATURE_InvalidAssemblyGapLocation,
                              "Invalid assembly_gap location : \"%s\".",
                              (fbp->location == NULL) ? "unknown" : fbp->location);
                ibp->drop = 1;
                break;
            }

            if(curr_gap == 2)           /* "assembly_gap" feature */
            {
                if(gap_type != NULL && is_htg > -1 &&
                   StringCmp(gap_type, "within scaffold") != 0 &&
                   StringCmp(gap_type, "repeat within scaffold") != 0)
                    ErrPostEx(SEV_ERROR, ERR_QUALIFIER_UnexpectedGapTypeForHTG,
                              "assembly_gap has /gap_type of \"%s\", but clone-based HTG records are only expected to have \"within scaffold\" or \"repeat within scaffold\" gaps. assembly_gap feature located at \"%d..%d\".",
                              gap_type, from, to);

                if(is_htg == 0 || is_htg == 1)
                {
                    ITERATE(std::list<std::string>, evidence, linkage_evidence_names)
                    {
                        if (*evidence != LinkageEvidenceValues[objects::CLinkage_evidence_Base::eType_unspecified].str)
                        {
                            ErrPostEx(SEV_ERROR, ERR_QUALIFIER_LinkageShouldBeUnspecified,
                                      "assembly gap has /linkage_evidence of \"%s\", but unoriented and unordered Phase0/Phase1 HTG records are expected to have \"unspecified\" evidence. assembly_gap feature located at \"%d..%d\".",
                                      evidence->c_str(), from, to);
                        }
                    }
                }
                else if(is_htg == 2 || is_htg == 3)
                {
                    ITERATE(std::list<std::string>, evidence, linkage_evidence_names)
                    {
                        if (*evidence != LinkageEvidenceValues[objects::CLinkage_evidence_Base::eType_unspecified].str)
                            continue;

                        ErrPostEx(SEV_ERROR, ERR_QUALIFIER_LinkageShouldNotBeUnspecified,
                                  "assembly gap has /linkage_evidence of \"unspecified\", but ordered and oriented HTG records are expected to have some level of linkage for their gaps. assembly_gap feature located at \"%d..%d\".",
                                  from, to);
                    }
                }

                if(is_htg == 3 && !finished_gap)
                {
                    ErrPostEx(SEV_ERROR, ERR_FEATURE_FinishedHTGHasAssemblyGap,
                              "Finished Phase-3 HTG records are not expected to have any gaps. First assembly_gap feature encountered at \"%d..%d\".",
                              from, to);
                    finished_gap = true;
                }

                if(gap_type == NULL)
                {
                    ErrPostEx(SEV_REJECT, ERR_QUALIFIER_MissingGapType,
                              "assembly_gap feature at \"%d..%d\" lacks the required /gap_type qualifier.",
                              from, to);
                    ibp->drop = 1;
                    break;
                }

                for(snp = GapTypeValues; snp->str != NULL; snp++)
                    if(StringCmp(snp->str, gap_type) == 0)
                        break;
                if(snp->str == NULL)
                {
                    ErrPostEx(SEV_REJECT, ERR_QUALIFIER_InvalidGapType,
                              "assembly_gap feature at \"%d..%d\" has an invalid gap type : \"%s\".",
                              from, to, gap_type);
                    ibp->drop = 1;
                    break;
                }
                asn_gap_type = snp->num;

                if(linkage_evidence_names.empty() &&
                   (StringCmp(gap_type, "within scaffold") == 0 ||
                   StringCmp(gap_type, "repeat within scaffold") == 0))
                {
                    ErrPostEx(SEV_REJECT, ERR_QUALIFIER_MissingLinkageEvidence,
                              "assembly_gap feature at \"%d..%d\" with gap type \"%s\" lacks a /linkage_evidence qualifier.",
                              from, to, gap_type);
                    ibp->drop = 1;
                    break;
                }
                if (!linkage_evidence_names.empty())
                {
                    if (StringCmp(gap_type, "unknown") != 0 &&
                        StringCmp(gap_type, "within scaffold") != 0 &&
                        StringCmp(gap_type, "repeat within scaffold") != 0)
                    {
                        ErrPostEx(SEV_REJECT,
                                  ERR_QUALIFIER_InvalidGapTypeForLinkageEvidence,
                                  "The /linkage_evidence qualifier is not legal for the assembly_gap feature at \"%d..%d\" with /gap_type \"%s\".",
                                  from, to, gap_type);
                        ibp->drop = 1;
                        break;
                    }

                    ITERATE(std::list<std::string>, evidence, linkage_evidence_names)
                    {
                        for(snp = LinkageEvidenceValues; snp->str != NULL; snp++)
                            if (*evidence == snp->str)
                                break;
                        if(snp->str == NULL)
                        {
                            ErrPostEx(SEV_REJECT,
                                      ERR_QUALIFIER_InvalidLinkageEvidence,
                                      "assembly_gap feature at \"%d..%d\" has an invalid linkage evidence : \"%s\".",
                                      from, to, evidence->c_str());
                            ibp->drop = 1;
                            break;
                        }
                        
                        CRef<objects::CLinkage_evidence> new_evidence(new objects::CLinkage_evidence);
                        new_evidence->SetType(snp->num);
                        asn_linkage_evidence.push_back(new_evidence);
                    }
                }
            }

            if(prev_gap + curr_gap == 3)
            {
                if(curr_gap == 1)
                    ErrPostEx(SEV_REJECT, ERR_FEATURE_AssemblyGapAndLegacyGap,
                              "Legacy gap feature at \"%d..%d\" co-exists with a new AGP 2.0 assembly_gap feature at \"%d..%d\".",
                              from, to, gfp->from, gfp->to);
                else
                    ErrPostEx(SEV_REJECT, ERR_FEATURE_AssemblyGapAndLegacyGap,
                              "Legacy gap feature at \"%d..%d\" co-exists with a new AGP 2.0 assembly_gap feature at \"%d..%d\".",
                              gfp->from, gfp->to, from, to);
                ibp->drop = 1;
                break;
            }

            if(estimated_length == -1)  /* missing qual */
            {
                ErrPostEx(SEV_REJECT, ERR_FEATURE_RequiredQualifierMissing,
                          "The gap feature at \"%d..%d\" lacks the required /estimated_length qualifier.",
                          from, to);
                ibp->drop = 1;
            }
            else if(estimated_length == 0)
            {
                ErrPostEx(SEV_REJECT, ERR_FEATURE_IllegalEstimatedLength,
                          "Gap feature at \"%d..%d\" has an illegal /estimated_length qualifier : \"%s\" : should be \"unknown\" or an integer.",
//                          from, to, gbqp->val); // at this point gbqp is definitely = NULL
                          from, to, "");
                ibp->drop = 1;
            }
            else if(estimated_length == -100)
            {
                if(is_htg >= 0 && to - from != 99)
                {
                    ErrPostEx(SEV_ERROR, ERR_FEATURE_UnknownGapNot100,
                              "Gap feature at \"%d..%d\" has /estimated_length \"unknown\" but the gap size is not 100 bases.",
                              from, to);
                }
            }
            else if(estimated_length != to - from + 1)
            {
                if(pp->source == Parser::ESource::EMBL || pp->source == Parser::ESource::DDBJ)
                    sev = SEV_ERROR;
                else
                {
                    sev = SEV_REJECT;
                    ibp->drop = 1;
                }

                ErrPostEx(sev, ERR_FEATURE_GapSizeEstLengthMissMatch,
                          "Gap feature at \"%d..%d\" has a size that does not match the /estimated_length : %d.",
                          from, to, estimated_length);
            }

            for(gfp = ibp->gaps; gfp != NULL; gfp = gfp->next)
            {
                if((gfp->from >= from && gfp->from <= to) ||
                   (gfp->to >= from && gfp->to <= to) ||
                   (gfp->from <= from && gfp->to >= to))
                {
                    ErrPostEx(SEV_REJECT, ERR_FEATURE_OverlappingGaps,
                              "Gap features at \"%d..%d\" and \"%d..%d\" overlap.",
                              from, to, gfp->from, gfp->to);
                    ibp->drop = 1;
                }
                else if(to + 1 == gfp->from || from - 1 == gfp->to)
                {
                    if(pp->source == Parser::ESource::EMBL)
                        sev = SEV_ERROR;
                    else
                    {
                        sev = SEV_REJECT;
                        ibp->drop = 1;
                    }

                    ErrPostEx(sev, ERR_FEATURE_ContiguousGaps,
                              "Gap features at \"%d..%d\" and \"%d..%d\" are contiguous, and should probably be represented by a single gap that spans both.",
                              from, to, gfp->from, gfp->to);
                }
            }
            if(ibp->drop != 0)
                break;

            gfp = new GapFeats;
            gfp->from = from;
            gfp->to = to;
            gfp->estimated_length = estimated_length;
            if(curr_gap == 2)           /* /assembly_gap feature */
                gfp->assembly_gap = true;
            if(gap_type != NULL)
            {
               gfp->gap_type = StringSave(gap_type);
               gfp->asn_gap_type = asn_gap_type;
            }
            if(!asn_linkage_evidence.empty())
            {
               gfp->asn_linkage_evidence.swap(asn_linkage_evidence);
               asn_linkage_evidence.clear();
            }
            gfp->next = NULL;

            if(ibp->gaps == NULL)
            {
                ibp->gaps = gfp;
                continue;
            }

            if(ibp->gaps->from > from)
            {
                gfp->next = ibp->gaps;
                ibp->gaps = gfp;
                continue;
            }

            if(ibp->gaps->next == NULL)
            {
                ibp->gaps->next = gfp;
                continue;
            }

            for(tgfp = ibp->gaps; tgfp != NULL; tgfp = tgfp->next)
            {
                if(tgfp->next != NULL && tgfp->next->from < from)
                    continue;
                gfp->next = tgfp->next;
                tgfp->next = gfp;
                break;
            }
        }
        if(ibp->drop != 0)
        {
            linkage_evidence_names.clear();
            asn_linkage_evidence.clear();
        }
    }

    if(ibp->gaps == NULL)
        return;

    if(ibp->drop != 0)
    {
        GapFeatsFree(ibp->gaps);
        ibp->gaps = NULL;
    }
}

/**********************************************************/
static void XMLGetQuals(char* entry, XmlIndexPtr xip, TQualVector& quals)
{
    XmlIndexPtr xipqual;

    if(entry == NULL || xip == NULL)
        return;

    for(; xip != NULL; xip = xip->next)
    {
        if(xip->subtags == NULL)
            continue;

        CRef<objects::CGb_qual> qual(new objects::CGb_qual);
        for(xipqual = xip->subtags; xipqual != NULL; xipqual = xipqual->next)
        {
            if(xipqual->tag == INSDQUALIFIER_NAME)
                qual->SetQual(XMLGetTagValue(entry, xipqual));
            else if(xipqual->tag == INSDQUALIFIER_VALUE)
                qual->SetVal(XMLGetTagValue(entry, xipqual));
        }

        if (qual->GetQual() == "replace" && !qual->IsSetVal())
        {
            qual->SetVal("");
        }

        if (qual->IsSetQual() && !qual->GetQual().empty())
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

    if(entry == NULL || xip == NULL)
        return(NULL);

    for(; xip != NULL; xip = xip->next)
        if(xip->tag == INSDSEQ_FEATURE_TABLE)
            break;

    if(xip == NULL || xip->subtags == NULL)
        return(NULL);

    headdbp = NULL;
    for(xip = xip->subtags; xip != NULL; xip = xip->next)
    {
        if(xip->subtags == NULL)
            continue;
        fbp = new FeatBlk;
        for(xipfeat = xip->subtags; xipfeat != NULL; xipfeat = xipfeat->next)
        {
            if(xipfeat->tag == INSDFEATURE_KEY)
                fbp->key = XMLGetTagValue(entry, xipfeat);
            else if(xipfeat->tag == INSDFEATURE_LOCATION)
                fbp->location = XMLGetTagValue(entry, xipfeat);
            else if(xipfeat->tag == INSDFEATURE_QUALS)
                XMLGetQuals(entry, xipfeat->subtags, fbp->quals);
        }
        if(headdbp == NULL)
        {
            headdbp = (DataBlkPtr) MemNew(sizeof(DataBlk));
            dbp = headdbp;
        }
        else
        {
            dbp->mpNext = (DataBlkPtr) MemNew(sizeof(DataBlk));
            dbp = dbp->mpNext;
        }
        dbp->mpData = fbp;
    }
    ret = (DataBlkPtr) MemNew(sizeof(DataBlk));
    ret->mType = XML_FEATURES;
    ret->mpData = headdbp;
    ret->mpNext = NULL;
    return(ret);
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
    char*   note;
    char*   p;
    char*   q;

    size_t size = 0;

    NON_CONST_ITERATE(TQualVector, cur, fbp->quals)
    {
        if (!(*cur)->IsSetQual() || !(*cur)->IsSetVal())
            continue;

        const std::string& cur_qual = (*cur)->GetQual();
        const std::string& cur_val = (*cur)->GetVal();

        if (cur_qual != "note" || cur_val.empty())
            continue;

        size += 2;
        std::vector<Char> buf(cur_val.size() + 1);

        const char* cp = cur_val.c_str();
        for(q = &buf[0]; *cp != '\0'; ++cp)
        {
            *q++ = *cp;
            if (*cp == ';' && (cp[1] == ' ' || cp[1] == ';'))
            {
                for(++cp; *cp == ' ' || *cp == ';';)
                    ++cp;
                if(*cp != '\0')
                    *q++ = ' ';
                --cp;
            }
        }

        *q = '\0';
        (*cur)->SetVal(&buf[0]);

        size += (*cur)->GetVal().size();
        for (cp = (*cur)->GetVal().c_str(); *cp != '\0'; ++cp)
            if(*cp == '~')
                ++size;
    }

    if(size == 0)
        return(fbp);

    note = (char*) MemNew(size);
    p = note;

    for (TQualVector::iterator cur = fbp->quals.begin(); cur != fbp->quals.end();)
    {
        if (!(*cur)->IsSetQual() || !(*cur)->IsSetVal())
        {
            ++cur;
            continue;
        }

        const std::string& cur_qual = (*cur)->GetQual();
        const std::string& cur_val = (*cur)->GetVal();

        if (cur_qual != "note")
        {
            ++cur;
            continue;
        }

        if (!cur_val.empty())
        {
            /* sometime we get note qual w/o value
             */
            if(p > note)
            {
                *p++ = ';';
                *p++ = '~';
            }

            for (const char* cq = cur_val.c_str(); *cq != '\0'; *p++ = *cq++)
                if(*cq == '~')
                    *p++ = '~';
        }

        cur = fbp->quals.erase(cur);
    }
    *p = '\0';

    CRef<objects::CGb_qual> qual_new(new objects::CGb_qual);
    qual_new->SetQual("note");
    qual_new->SetVal(note);

    fbp->quals.push_back(qual_new);

    return(fbp);
}

/**********************************************************/
static bool CheckLegalQual(const Char* val, Char ch, std::string* qual)
{
    std::string qual_name;
    for (; *val && *val != ch && (isalpha(*val) || *val == '_'); ++val)
        qual_name += *val;

    objects::CSeqFeatData::EQualifier type = objects::CSeqFeatData::GetQualifierType(qual_name);
    if (type == objects::CSeqFeatData::eQual_bad)
        return false;

    if (qual != nullptr)
        *qual = qual_name;

    return true;
}

/**********************************************************/
static void fta_set_merge_marks(char* val, size_t quallen, size_t vallen)
{
    char* start;
    char* p;
    char* q;
    bool    first;

    if(val == NULL || *val == '\0')
        return;

    p = StringChr(val, '\n');
    if(p == NULL)
        return;

    for(first = true, start = val; p != NULL;)
    {
        if((p - 1) >= start && *(p - 1) == '-' &&
           (p - 2) >= start && *(p - 2) != ' ')
        {
            *p = '\t';
            start = ++p;
            p = StringChr(p, '\n');
            continue;
        }
        if((p - 3) >= start && StringNCmp(p - 3, "(EC", 3) == 0 &&
           p[1] >= '0' && p[1] <= '9')
        {
            start = ++p;
            p = StringChr(p, '\n');
            continue;
        }
        if(p[1] == '(' || ((p - 1) >= start && *(p - 1) == ','))
        {
            start = ++p;
            p = StringChr(p, '\n');
            continue;
        }
        *p = '\0';
        q = StringChr(start, ' ');
        size_t len = StringLen(start);
        if(first)
        {
            first = false;
            len += quallen;
        }
        *p = (q == NULL && len == vallen) ? '\t' : '\n';
        start = ++p;
        p = StringChr(p, '\n');
    }
}

/**********************************************************/
static void fta_convert_to_lower_case(char* str)
{
    char* p;

    if (str == NULL || *str == '\0')
        return;

    for (p = str; *p != '\0'; p++)
        if (*p >= 'A' && *p <= 'Z')
            *p |= 040;
}

/**********************************************************/
static void fta_process_con_slice(std::vector<char>& val_buf)
{
    size_t i = 1;
    char* p = &val_buf[0];

    for (; *p != '\0'; p++)
        if (*p == ',' && p[1] != ' ' && p[1] != '\0')
            i++;

    if (i > 1)
    {
        vector<char> buf(i + val_buf.size());
        char* q = &buf[0];
        for (p = &val_buf[0]; *p != '\0'; p++)
        {
            *q++ = *p;
            if (*p == ',' && p[1] != ' ' && p[1] != '\0')
                *q++ = ' ';
        }
        *q = '\0';
        val_buf.swap(buf);
    }
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
static void ParseQualifiers(FeatBlkPtr fbp, char* bptr, char* eptr,
                            Parser::EFormat format)
{
    const char **b;

    char*    ptr;
    char*    str;
    char*    qstr;
    char*    p;
    char*    q;
    char*    r;
    Char       ch;
    Int4       vallen;
    Int4       count;
    Int2       got;
    Int2       quotes;
    Int2       reject;

    vallen = (format == Parser::EFormat::EMBL) ? 59 : 58;

    qstr = (char*) MemNew(eptr - bptr + 2);
    ch = *eptr;
    *eptr = '\0';

    for(p = bptr; *p == ' ' || *p == '\n';)
        p++;
    for(q = qstr; *p != '\0';)
    {
        if(*p != ' ' && *p != '\n')
        {
            *q++ = *p++;
            continue;
        }

        for(got = 0, r = p; *r == ' ' || *r == '\n'; r++)
            if(*r == '\n')
                got = 1;
        if(got == 1)
        {
            *q++ = '\n';
            p = r;
        }
        else
            while(*p == ' ')
                *q++ = *p++;
    }
    if(q == qstr || *(q - 1) != '\n')
        *q++ = '\n';
    *q = '\0';
    *eptr = ch;

    for(str = qstr + 1; *str != '\0';)
    {
        reject = 0;

        CRef<objects::CGb_qual> qual_new(new objects::CGb_qual);
        for(ptr = str; *str != '/' && *str != '=' && *str != '\0' && *str != '\n';)
            str++;

        std::string qual_str(ptr, str);
        size_t quallen = qual_str.size() + 1;

        NStr::ReplaceInPlace(qual_str, "\n", " ");
        NStr::TruncateSpacesInPlace(qual_str, NStr::eTrunc_End);

        if (qual_str == "specific_host")
            qual_str = "host";
        qual_new->SetQual(qual_str);

        quotes = 0;
        if(*str == '=')                 /* get gbq->val */
        {
            quallen++;
            while(*str == '=' || *str == ' ' || *str == '\n')
                str++;

            if(*str == '\"')            /* found open double quote */
            {
                quallen++;
                quotes = 1;
                str++;
                ptr = str;

                /* search first close double quote
                 */
                if (qual_str == "note")
                {
                    for(;;)
                    {
                        str = StringChr(str, '\n');
                        if(str[1] == '\0')
                        {
                            if(*(str - 1) == '\"')
                            {
                                quotes++;
                                str--;
                            }
                            break;
                        }
                        if (str[1] != '/' || !CheckLegalQual(str + 2, '\n', nullptr))
                        {
                            str++;
                            continue;
                        }
                        if(*(str - 1) == '\"')
                        {
                            quotes++;
                            str--;
                        }
                        break;
                    }
                }
                else
                {
                    while(*str != '\"' && *str != '\0')
                        str++;
                }
            }
            else
            {
                for(ptr = str; *str != '\0'; str++)
                    if(*str == '\n' && str[1] == '/')
                    {
                        str++;
                        break;
                    }
            }

            std::vector<Char> val_buf(ptr, str);
            val_buf.push_back(0);

            if (!val_buf.empty())
            {
                fta_set_merge_marks(&val_buf[0], quallen, vallen);

                std::replace(val_buf.begin(), val_buf.end(), '\n', ' ');
                val_buf.erase(std::remove(val_buf.begin(), val_buf.end(), '\t'), val_buf.end());

                std::string aux(&val_buf[0]);
                NStr::TruncateSpacesInPlace(aux, NStr::eTrunc_End);
                val_buf.assign(aux.begin(), aux.end());
                val_buf.push_back(0);

                if(qual_str == "translation" ||
                   qual_str == "replace")
                {
                    /* delete blanks in the middle of the data
                     */
                    val_buf.erase(std::remove(val_buf.begin(), val_buf.end(), ' '), val_buf.end());
                }
                else if(qual_str == "rpt_unit")
                {
                    fta_convert_to_lower_case(&val_buf[0]);
                }
                else if (qual_str == "cons_splice")
                {
                    fta_process_con_slice(val_buf);
                }
                else if (qual_str ==  "note")
                {
                    if(quotes == 1)
                    {
                        if (val_buf.size() > 30)
                        {
                            ch = val_buf[30];
                            val_buf[30] = '\0';
                        }
                        else
                            ch = '\0';
                        ErrPostEx(SEV_WARNING,
                                  ERR_QUALIFIER_MissingTerminalDoubleQuote,
                                  "/note qualifier is not terminated with double quote : [%s%s].",
                                  &val_buf[0], (ch == '\0') ? "" : " ...");
                        if(ch != '\0')
                            val_buf[30] = ch;
                    }
                    for (quotes = 0, p = &val_buf[0]; *p != '\0'; p++)
                    {
                        if(*p != '\"')
                            continue;

                        if(p[1] != '\"')
                        {
                            quotes = 1;
                            break;
                        }
                        quotes = !quotes;
                        p++;
                    }
                    if(quotes != 0)
                    {
                        if (val_buf.size() > 30)
                        {
                            ch = val_buf[30];
                            val_buf[30] = '\0';
                        }
                        else
                            ch = '\0';
                        ErrPostEx(SEV_ERROR, ERR_QUALIFIER_UnbalancedQuotes,
                                  "/note qualifier value contains unbalanced double-quotes, and has been discarded : [%s%s].",
                                  &val_buf[0], (ch == '\0') ? "" : " ...");
                        if(ch != '\0')
                            val_buf[30] = ch;
                        reject = 1;
                    }

                    if(fbp != NULL && fbp->key != NULL &&
                       StringCmp(fbp->key, "misc_feature") != 0)
                    {
                        std::string qual;
                        for (count = 0, p = &val_buf[0]; ; p++)
                        {
                            p = StringChr(p, '/');
                            if(p == NULL)
                                break;

                            std::string cur_qual;
                            if (CheckLegalQual(p + 1, ' ', &cur_qual))
                            {
                                if (qual.empty())
                                    qual = cur_qual;
                                else
                                    count++;
                            }
                        }

                        if (!qual.empty())
                        {
                            FtaDeletePrefix(PREFIX_FEATURE);
                            if(count == 0)
                                ErrPostEx(SEV_WARNING, ERR_QUALIFIER_EmbeddedQual,
                                          "/note contains /%s : FEAT=%s[%s] : %s.",
                                          qual.c_str(), fbp->key, fbp->location, &val_buf[0]);
                            else
                                ErrPostEx(SEV_WARNING, ERR_QUALIFIER_EmbeddedQual,
                                          "/note contains /%s and %d other embedded qualifiers : FEAT=%s[%s] : %s.",
                                          qual.c_str(), count, fbp->key, fbp->location, &val_buf[0]);
                            FtaInstallPrefix(PREFIX_FEATURE, fbp->key,
                                             fbp->location);
                        }
                    }
                }

                qual_new->SetVal(&val_buf[0]);
            }

            while(*str == ' ' || *str == '\"' || *str == '\n')
                str++;

            /* check any truncated data
             */
            if(*str != '\0' && *str != '/')
            {
                for(ptr = str; *str != '/' && *str != '\0';)
                    str++;

                std::string aux(ptr, str);
                if(str - ptr > 50)
                    aux.resize(50);
                NStr::ReplaceInPlace(aux, "\n", " ");

                ErrPostEx(SEV_WARNING, ERR_FEATURE_DiscardData, "%s", aux.c_str());
            }
        } /* if, = */

        while(*str == ' ' || *str == '/' || *str == '\"' || *str == '\n')
            str++;

        if(reject != 0)
            continue;

        if (qual_new->IsSetVal())
        {
            const std::string& val_str = qual_new->GetVal();
            const char* cp = val_str.c_str();
            for(; *cp == '\"' || *cp == ' ' || *cp == '\t';)
                ++cp;
            if(*cp == '\0')
            {
                if(qual_str == "replace")
                    qual_new->SetVal("");
                else
                    qual_new->ResetVal();
            }
        }

        for(b = EmptyQuals; *b != NULL; b++)
            if (qual_str == *b)
                break;

        if(*b == NULL)
        {
            if (!qual_new->IsSetVal())
            {
                if (qual_str == "old_locus_tag")
                    ErrPostEx(SEV_ERROR, ERR_FEATURE_EmptyOldLocusTag,
                              "Feature \"%s\" at \"%s\" has an /old_locus_tag qualifier with no value. Qualifier has been dropped.",
                              (fbp->key == NULL) ? "Unknown" : fbp->key,
                              (fbp->location == NULL) ? "Empty" : fbp->location);
                else
                    ErrPostEx(SEV_WARNING, ERR_QUALIFIER_EmptyQual,
                              "Qualifier /%s ignored because it lacks a data value. Feature \"%s\", location \"%s\".",
                              qual_str.c_str(),
                              (fbp->key == NULL) ? "Unknown" : fbp->key,
                              (fbp->location == NULL) ? "Empty" : fbp->location);
                continue;
            }
        }
        else if (qual_new->IsSetVal())
        {
            if (qual_str != "artificial_location" &&
                qual_str != "mobile_element_type")
            {
                ErrPostEx(SEV_WARNING, ERR_QUALIFIER_ShouldNotHaveValue,
                          "Qualifier /%s should not have data value. Qualifier value has been ignored. Feature \"%s\", location \"%s\".",
                          qual_str.c_str(), (fbp->key == NULL) ? "Unknown" : fbp->key,
                          (fbp->location == NULL) ? "Empty" : fbp->location);
                qual_new->ResetVal();
            }
        }

        if (qual_new->IsSetVal() && qual_str == "note")
        {
            std::string val = qual_new->GetVal();
            std::replace(val.begin(), val.end(), '\"', '\'');
            qual_new->SetVal(val);
        }

        if (qual_new->IsSetQual() && !qual_new->GetQual().empty())
            fbp->quals.push_back(qual_new);
    }

    MemFree(qstr);
}

/**********************************************************/
static void fta_check_satellite(char* str, unsigned char* drop)
{
    char* p;
    Int2    i;

    if(str == NULL || *str == '\0')
        return;

    p = StringChr(str, ':');
    if(p != NULL)
        *p = '\0';

    i = MatchArrayString(SatelliteValues, str);
    if(p != NULL)
        *p = ':';
    if(i < 0)
    {
        ErrPostEx(SEV_REJECT, ERR_FEATURE_InvalidSatelliteType,
                  "/satellite qualifier \"%s\" does not begin with a valid satellite type.",
                  str);
        *drop = 1;
    }
    else if(p != NULL && p[1] == '\0')
    {
        ErrPostEx(SEV_REJECT, ERR_FEATURE_NoSatelliteClassOrIdentifier,
                  "/satellite qualifier \"%s\" does not include a class or identifier after the satellite type.",
                  str);
        *drop = 1;
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
 *      fdbp->drop = 1, if found unknown feature key, or
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
int ParseFeatureBlock(IndexblkPtr ibp, bool deb, DataBlkPtr dbp,
                      Parser::ESource source, Parser::EFormat format)
{
    char*    bptr;
    char*    eptr;
    char*    ptr1;
    char*    ptr2;
    char*    p;
    char*    q;
    Char       loc[100];
    Char       ch;

    FeatBlkPtr fbp;
    Int4       num;
    size_t     i;
    int        retval = GB_FEAT_ERR_NONE;
    int        ret;

    if(ibp->is_mga)
        sprintf(loc, "1..%ld", ibp->bases);
    for(num = 0; dbp != NULL; dbp = dbp->mpNext, num++)
    {
        fbp = new FeatBlk;
        fbp->num = num;
        dbp->mpData = fbp;

        bptr = dbp->mOffset;
        eptr = bptr + dbp->len;

        for(p = bptr; *p != '\n';)
            p++;
        *p = '\0';
        FtaInstallPrefix(PREFIX_FEATURE, (char *) "Parsing FT line: ", bptr);
        *p = '\n';
        ptr1 = bptr + ParFlat_COL_FEATKEY;
        if(*ptr1 == ' ')
        {
            ErrPostStr(SEV_WARNING, ERR_FEATURE_FeatureKeyReplaced,
                       "Empty featkey");
        }
        for(ptr1 = bptr; *ptr1 == ' ';)
            ptr1++;

        for(ptr2 = ptr1; *ptr2 != ' ' && *ptr2 != '\n';)
            ptr2++;

        if(StringNCmp(ptr1, "- ", 2) == 0)
        {
            ErrPostStr(SEV_WARNING, ERR_FEATURE_FeatureKeyReplaced,
                       "Featkey '-' is replaced by 'misc_feature'");
            fbp->key = StringSave("misc_feature");
        }
        else
            fbp->key = StringSave(std::string(ptr1, ptr2).c_str());

        for(ptr1 = ptr2; *ptr1 == ' ';)
            ptr1++;
        if(*ptr1 == '\n')
        {
            if(ibp->is_mga == false)
            {
                ErrPostEx(SEV_WARNING, ERR_FEATURE_LocationParsing,
                          "Location missing");
                dbp->mDrop = true;
                retval = GB_FEAT_ERR_DROP;
                continue;
            }
        }
        else
        {
            i = ptr1 - bptr;
            if(i < ParFlat_COL_FEATDAT)
                ErrPostEx(SEV_WARNING, ERR_FEATURE_LocationParsing,
                          "Location data is shifted to the left");
            else if(i > ParFlat_COL_FEATDAT)
                ErrPostEx(SEV_WARNING, ERR_FEATURE_LocationParsing,
                          "Location data is shifted to the right");
        }

        for(ptr2 = ptr1; *ptr2 != '/' && ptr2 < eptr;)
            ptr2++;
        ch = *ptr2;
        *ptr2 = '\0';
        fbp->location = StringSave(ptr1);
        if(ibp->is_prot)
            fta_strip_aa(fbp->location);
        *ptr2 = ch;
        for(p = fbp->location, q = p; *p != '\0'; p++)
            if(*p != ' ' && *p != '\n')
                *q++ = *p;
        *q = '\0';

        if(fbp->location[0] == '\0' && ibp->is_mga)
        {
            MemFree(fbp->location);
            fbp->location = StringSave(loc);
        }

        FtaInstallPrefix(PREFIX_FEATURE, fbp->key, fbp->location);
        if(StringCmp(fbp->key, "allele") == 0 ||
           StringCmp(fbp->key, "mutation") == 0)
        {
            ErrPostEx(SEV_ERROR, ERR_FEATURE_ObsoleteFeature,
                      "Obsolete feature \"%s\" found. Replaced with \"variation\".",
                      fbp->key);
            MemFree(fbp->key);
            fbp->key = StringSave("variation");
        }

        objects::CSeqFeatData::ESubtype subtype = objects::CSeqFeatData::SubtypeNameToValue(fbp->key);

        if (subtype == objects::CSeqFeatData::eSubtype_bad && !deb)
        {
            ErrPostEx(SEV_ERROR, ERR_FEATURE_UnknownFeatKey, fbp->key,
                      "Feature dropped");
            dbp->mDrop = true;
            retval = GB_FEAT_ERR_DROP;
            continue;
        }

        if(*ptr2 == '/')                /* qualifier start in first "/" */
        {
            ParseQualifiers(fbp, ptr2, eptr, format);

            if(StringCmp(fbp->key, "assembly_gap") != 0)
            {
                ITERATE(TQualVector, cur, fbp->quals)
                {
                    const std::string& cur_qual = (*cur)->GetQual();
                    if (cur_qual == "gap_type" ||
                        cur_qual == "assembly_evidence")
                    {
                        ErrPostEx(SEV_REJECT, ERR_FEATURE_InvalidQualifier,
                                  "Qualifier /%s is invalid for the feature \"%s\" at \"%s\".",
                                  cur_qual.c_str(), fbp->key, (fbp->location == NULL) ? "Unknown" : fbp->location);
                        ibp->drop = 1;
                    }
                }
            }

            if(StringCmp(fbp->key, "source") != 0)
            {
                ITERATE(TQualVector, cur, fbp->quals)
                {
                    const std::string& cur_qual = (*cur)->GetQual();
                    if (cur_qual == "submitter_seqid" )
                    {
                        ErrPostEx(SEV_REJECT, ERR_FEATURE_InvalidQualifier,
                                  "Qualifier /%s is invalid for the feature \"%s\" at \"%s\".",
                                  cur_qual.c_str(), fbp->key, (fbp->location == NULL) ? "Unknown" : fbp->location);
                        ibp->drop = 1;
                    }
                }
            }

            fbp = MergeNoteQual(fbp);   /* allow more than one
                                           notes w/i a key */

            if (subtype == objects::CSeqFeatData::eSubtype_bad)
            {
                ErrPostStr(SEV_ERROR, ERR_FEATURE_UnknownFeatKey, fbp->key);
                ret = GB_FEAT_ERR_REPAIRABLE;
            }
            else
            {
                /* last argument is perform_corrections if debug
                 * mode is FALSE
                 */
                ret = XGBFeatKeyQualValid(subtype, fbp->quals, true, (source == Parser::ESource::Flybase ? false : !deb));
            }
            if(ret > retval)
                retval = ret;

            if(ret > GB_FEAT_ERR_REPAIRABLE &&
               StringCmp(fbp->key, "ncRNA") != 0)
                dbp->mDrop = true;
        }
        else if (subtype == objects::CSeqFeatData::eSubtype_bad && !objects::CSeqFeatData::GetMandatoryQualifiers(subtype).empty())
        {
            if(StringCmp(fbp->key, "mobile_element") != 0)
            {
                auto qual_idx = *objects::CSeqFeatData::GetMandatoryQualifiers(subtype).begin();
                std::string str1 = objects::CSeqFeatData::GetQualifierAsString(qual_idx);
                const char *str = str1.c_str();
                if((StringCmp(fbp->key, "old_sequence") != 0 &&
                    StringCmp(fbp->key, "conflict") != 0) ||
                   StringCmp(str, "citation") != 0)
                {
                    ErrPostEx(SEV_ERROR, ERR_FEATURE_RequiredQualifierMissing,
                              "lacks required /%s qualifier : feature has been dropped.",
                              str);
                    if(!deb)
                    {
                        dbp->mDrop = true;
                        retval = GB_FEAT_ERR_DROP;
                    }
                }
            }
        }
        else if(StringCmp(fbp->key, "misc_feature") == 0 && fbp->quals.empty())
        {
            if (!deb)
            {
                dbp->mDrop = true;
                retval = GB_FEAT_ERR_DROP;
                ErrPostStr(SEV_WARNING, ERR_FEATURE_Dropped,
                           "Empty 'misc_feature' dropped");
            }
            else
                retval = GB_FEAT_ERR_REPAIRABLE;
        }

        NON_CONST_ITERATE(TQualVector, cur, fbp->quals)
        {
            if (!(*cur)->IsSetQual() || !(*cur)->IsSetVal())
                continue;

            const std::string& qual_str = (*cur)->GetQual();
            const std::string& val_str = (*cur)->GetVal();

            std::vector<Char> val_buf(val_str.begin(), val_str.end());
            val_buf.push_back(0);

            p = &val_buf[0];
            ShrinkSpaces(p);
            if (*p == '\0' && qual_str != "replace")
            {
                (*cur)->ResetVal();
                val_buf[0] = 0;
            }
            else
            {
                if (qual_str == "replace")
                    fta_convert_to_lower_case(p);
                (*cur)->SetVal(p);
            }

            if (qual_str == "satellite")
                fta_check_satellite(&val_buf[0], &ibp->drop);
        }
    } /* for, each sub-block, or each feature key */
    FtaDeletePrefix(PREFIX_FEATURE);
    return(retval);
}

/**********************************************************/
static void XMLCheckQualifiers(FeatBlkPtr fbp)
{
    const char **b;
    char*    p;
    Char       ch;

    if(fbp == NULL || fbp->quals.empty())
        return;

    for (TQualVector::iterator cur = fbp->quals.begin(); cur != fbp->quals.end();)
    {
        const std::string& qual_str = (*cur)->GetQual();

        if ((*cur)->IsSetVal())
        {
            const std::string& val_str = (*cur)->GetVal();
            std::vector<Char> val_buf(val_str.begin(), val_str.end());
            val_buf.push_back(0);

            if (qual_str == "translation")
            {
                DelCharBtwData(&val_buf[0]);
            }
            else if (qual_str == "rpt_unit")
            {
                fta_convert_to_lower_case(&val_buf[0]);
            }
            else if (qual_str == "cons_splice")
            {
                fta_process_con_slice(val_buf);
            }
            else if (qual_str == "note")
            {
                for(p = &val_buf[0];;)
                {
                    p = StringChr(p, '/');
                    if(p == NULL)
                        break;
                    p++;
                    if (!CheckLegalQual(p, ' ', nullptr))
                        continue;

                    if (val_buf.size() > 30)
                    {
                        ch = val_buf[30];
                        val_buf[30] = '\0';
                    }
                    else
                        ch = '\0';
                    ErrPostEx(SEV_WARNING, ERR_QUALIFIER_EmbeddedQual,
                              "/note qualifier value appears to contain other qualifiers : [%s%s].",
                              &val_buf[0], (ch == '\0') ? "" : " ...");
                    if(ch != '\0')
                        val_buf[30] = ch;
                }
            }

            for (p = &val_buf[0]; *p == '\"' || *p == ' ' || *p == '\t';)
                p++;

            if(*p == '\0')
            {
                if (qual_str == "replace")
                {
                    (*cur)->SetVal("");
                }
                else
                    (*cur)->ResetVal();
            }
            else
                (*cur)->SetVal(&val_buf[0]);
        }

        for (b = EmptyQuals; *b != NULL; b++)
            if (qual_str == *b)
                break;

        if (*b == NULL)
        {
            if (!(*cur)->IsSetVal())
            {
                if (qual_str == "old_locus_tag")
                    ErrPostEx(SEV_ERROR, ERR_FEATURE_EmptyOldLocusTag,
                              "Feature \"%s\" at \"%s\" has an /old_locus_tag qualifier with no value. Qualifier has been dropped.",
                              (fbp->key == NULL) ? "Unknown" : fbp->key,
                              (fbp->location == NULL) ? "Empty" : fbp->location);
                else
                    ErrPostEx(SEV_WARNING, ERR_QUALIFIER_EmptyQual,
                              "Qualifier /%s ignored because it lacks a data value. Feature \"%s\", location \"%s\".",
                              qual_str.c_str(),
                              (fbp->key == NULL) ? "Unknown" : fbp->key,
                              (fbp->location == NULL) ? "Empty" : fbp->location);

                cur = fbp->quals.erase(cur);
                continue;
            }
        }
        else if ((*cur)->IsSetVal())
        {
            ErrPostEx(SEV_WARNING, ERR_QUALIFIER_ShouldNotHaveValue,
                      "Qualifier /%s should not have data value. Qualifier value has been ignored. Feature \"%s\", location \"%s\".",
                      qual_str.c_str(), (fbp->key == NULL) ? "Unknown" : fbp->key,
                      (fbp->location == NULL) ? "Empty" : fbp->location);

            (*cur)->ResetVal();
        }

        if ((*cur)->IsSetVal() && qual_str == "note")
        {
            std::string val = (*cur)->GetVal();
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
    char*    p;
    Int4       num;
    int        retval = GB_FEAT_ERR_NONE;
    int        ret;

    for(num = 0; dbp != NULL; dbp = dbp->mpNext, num++)
    {
        if(dbp->mpData == NULL)
            continue;
        fbp = (FeatBlkPtr) dbp->mpData;
        fbp->num = num;
        FtaInstallPrefix(PREFIX_FEATURE, fbp->key, fbp->location);

        if(fbp->key[0] == '-' && fbp->key[1] == '\0')
        {
            ErrPostStr(SEV_WARNING, ERR_FEATURE_FeatureKeyReplaced,
                       "Featkey '-' is replaced by 'misc_feature'");
            MemFree(fbp->key);
            fbp->key = StringSave("misc_feature");
        }

        if(StringCmp(fbp->key, "allele") == 0 ||
           StringCmp(fbp->key, "mutation") == 0)
        {
            ErrPostEx(SEV_ERROR, ERR_FEATURE_ObsoleteFeature,
                      "Obsolete feature \"%s\" found. Replaced with \"variation\".",
                      fbp->key);
            MemFree(fbp->key);
            fbp->key = StringSave("variation");
        }

        objects::CSeqFeatData::ESubtype subtype = objects::CSeqFeatData::SubtypeNameToValue(fbp->key);

        if (subtype == objects::CSeqFeatData::eSubtype_bad && !deb)
        {
            ErrPostEx(SEV_ERROR, ERR_FEATURE_UnknownFeatKey, fbp->key,
                      "Feature dropped");
            dbp->mDrop = true;
            retval = GB_FEAT_ERR_DROP;
            continue;
        }

        if (!fbp->quals.empty())
        {
            XMLCheckQualifiers(fbp);
            fbp = MergeNoteQual(fbp);   /* allow more than one
                                           notes w/i a key */

            if (subtype == objects::CSeqFeatData::eSubtype_bad)
            {
                ErrPostStr(SEV_ERROR, ERR_FEATURE_UnknownFeatKey, fbp->key);
                ret = GB_FEAT_ERR_REPAIRABLE;
            }
            else
            {
                /* last argument is perform_corrections if debug
                 * mode is FALSE
                 */
                ret = XGBFeatKeyQualValid(subtype, fbp->quals, true, ((source == Parser::ESource::Flybase) ? false : !deb));
            }
            if(ret > retval)
                retval = ret;

            if(ret > GB_FEAT_ERR_REPAIRABLE &&
               StringCmp(fbp->key, "ncRNA") != 0)
                dbp->mDrop = true;
        }
        else if (subtype == objects::CSeqFeatData::eSubtype_bad && !objects::CSeqFeatData::GetMandatoryQualifiers(subtype).empty())
        {
            if(StringCmp(fbp->key, "mobile_element") != 0)
            {
                auto qual_idx = *objects::CSeqFeatData::GetMandatoryQualifiers(subtype).begin();
                std::string str1 = objects::CSeqFeatData::GetQualifierAsString(qual_idx);
                const char *str = str1.c_str();
                if((StringCmp(fbp->key, "old_sequence") != 0 &&
                    StringCmp(fbp->key, "conflict") != 0) ||
                   StringCmp(str, "citation") != 0)
                {
                    ErrPostEx(SEV_ERROR, ERR_FEATURE_RequiredQualifierMissing,
                              "lacks required /%s qualifier : feature has been dropped.",
                              str);
                    if(!deb)
                    {
                        dbp->mDrop = true;
                        retval = GB_FEAT_ERR_DROP;
                    }
                }
            }
        }
        else if(StringCmp(fbp->key, "misc_feature") == 0 && fbp->quals.empty())
        {
            if (!deb)
            {
                dbp->mDrop = true;
                retval = GB_FEAT_ERR_DROP;
                ErrPostStr(SEV_WARNING, ERR_FEATURE_Dropped,
                           "Empty 'misc_feature' dropped");
            }
            else
                retval = GB_FEAT_ERR_REPAIRABLE;
        }

        NON_CONST_ITERATE(TQualVector, cur, fbp->quals)
        {
            if (!(*cur)->IsSetQual() || !(*cur)->IsSetVal())
                continue;

            const std::string& qual_str = (*cur)->GetQual();
            const std::string& val_str = (*cur)->GetVal();

            std::vector<Char> val_buf(val_str.begin(), val_str.end());
            val_buf.push_back(0);

            p = &val_buf[0];
            ShrinkSpaces(p);
            if (*p == '\0' && qual_str != "replace")
            {
                (*cur)->ResetVal();
                val_buf[0] = 0;
            }
            else
            {
                if (qual_str == "replace")
                    fta_convert_to_lower_case(p);
                (*cur)->SetVal(p);
            }
        }
    } /* for, each sub-block, or each feature key */
    FtaDeletePrefix(PREFIX_FEATURE);
    return(retval);
}

/**********************************************************/
static bool fta_check_ncrna(const objects::CSeq_feat& feat)
{
    int count = 0;

    bool stop = false;
    ITERATE(objects::CSeq_feat::TQual, qual, feat.GetQual())
    {
        if (!(*qual)->IsSetQual() || (*qual)->GetQual().empty() ||
            (*qual)->GetQual() != "ncRNA_class")
            continue;

        count++;

        if (!(*qual)->IsSetVal() || (*qual)->GetVal().empty())
        {
            string loc = location_to_string_or_unknown(feat.GetLocation());

            ErrPostEx(SEV_REJECT, ERR_FEATURE_ncRNA_class,
                      "Feature \"ncRNA\" at location \"%s\" has an empty /ncRNA_class qualifier.",
                      loc.empty() ? "unknown" : loc.c_str());
            stop = true;
            break;
        }

        if (MatchArrayString(ncRNA_class_values, (*qual)->GetVal().c_str()) < 0)
        {
            string loc = location_to_string_or_unknown(feat.GetLocation());

            ErrPostEx(SEV_REJECT, ERR_FEATURE_ncRNA_class,
                      "Feature \"ncRNA\" at location \"%s\" has an invalid /ncRNA_class qualifier: \"%s\".",
                      loc.empty() ? "unknown" : loc.c_str(), (*qual)->GetVal().c_str());
            stop = true;
            break;
        }
    }

    if (stop)
        return false;

    if (count == 1)
        return true;

    string loc = location_to_string_or_unknown(feat.GetLocation());

    ErrPostEx(SEV_REJECT, ERR_FEATURE_ncRNA_class,
              "Feature \"ncRNA\" at location \"%s\" %s /ncRNA_class qualifier.",
              loc.empty() ? "unknown" : loc.c_str(),
              (count == 0) ? "lacks the mandatory" : "has more than one");

    return false;
}

/**********************************************************/
static void fta_check_artificial_location(objects::CSeq_feat& feat, char* key)
{
    NON_CONST_ITERATE(objects::CSeq_feat::TQual, qual, feat.SetQual())
    {
        if (!(*qual)->IsSetQual() || (*qual)->GetQual() != "artificial_location")
            continue;

        if ((*qual)->IsSetVal())
        {
            const Char* p_val = (*qual)->GetVal().c_str();
            for (; *p_val == '\"';)
                ++p_val;

            if (*p_val == '\0')
                (*qual)->ResetVal();
        }

        std::string val = (*qual)->IsSetVal() ? (*qual)->GetVal() : "";

        if (val == "heterogenous population sequenced" ||
            val == "low-quality sequence region")
        {
            feat.SetExcept(true);

            if (!feat.IsSetExcept_text())
                feat.SetExcept_text(val);
            else
            {
                std::string& except_text = feat.SetExcept_text();
                except_text += ", ";
                except_text += val;
            }
        }
        else
        {
            auto loc_str = location_to_string_or_unknown(feat.GetLocation());

            if (val.empty())
                ErrPostEx(SEV_ERROR, ERR_QUALIFIER_InvalidArtificialLoc,
                          "Encountered empty /artificial_location qualifier : Feature \"%s\" : Location \"%s\". Qualifier dropped.",
                          (key == NULL || *key == '\0') ? "unknown" : key,
                          loc_str.empty() ? "unknown" : loc_str.c_str());
            else
                ErrPostEx(SEV_ERROR, ERR_QUALIFIER_InvalidArtificialLoc,
                          "Value \"%s\" is not legal for the /artificial_location qualifier : Feature \"%s\" : Location \"%s\". Qualifier dropped.",
                          val.c_str(),
                          (key == NULL || *key == '\0') ? "unknown" : key,
                          loc_str.empty() ? "unknown" : loc_str.c_str());
        }

        feat.SetQual().erase(qual);
        break;
    }
}

/**********************************************************/
static bool fta_check_mobile_element(const objects::CSeq_feat& feat)
{
    bool found = false;
    ITERATE(objects::CSeq_feat::TQual, qual, feat.GetQual())
    {
        if ((*qual)->IsSetQual() && (*qual)->GetQual() == "mobile_element_type" &&
            (*qual)->IsSetVal() && !(*qual)->GetVal().empty())
        {
            const Char* p_val = (*qual)->GetVal().c_str();
            for (; *p_val == '\"';)
                ++p_val;

            if (*p_val != '\0')
            {
                found = true;
                break;
            }
        }
    }

    if (found)
        return true;

    auto loc_str = location_to_string_or_unknown(feat.GetLocation());
    ErrPostEx(SEV_REJECT, ERR_FEATURE_RequiredQualifierMissing,
              "Mandatory qualifier /mobile_element_type is absent or has no value : Feature \"mobile_element\" : Location \"%s\". Entry dropped.",
              loc_str.empty() ? "unknown" : loc_str.c_str());

    return false;
}

/**********************************************************/
static bool SortFeaturesByLoc(const DataBlkPtr& sp1, const DataBlkPtr& sp2)
{
    FeatBlkPtr      fbp1;
    FeatBlkPtr      fbp2;
    Int4            status;

    fbp1 = (FeatBlkPtr) sp1->mpData;
    fbp2 = (FeatBlkPtr) sp2->mpData;

    if(fbp1->location == NULL && fbp2->location != NULL)
        return false;
    if(fbp1->location != NULL && fbp2->location == NULL)
        return false;

    if (fbp1->location != NULL && fbp2->location != NULL)
    {
        status = StringCmp(fbp1->location, fbp2->location);
        if (status != 0)
            return status < 0;
    }

    if(fbp1->key == NULL && fbp2->key != NULL)
        return false;
    if (fbp1->key != NULL && fbp2->key == NULL)
        return false;
    if (fbp1->key != NULL && fbp2->key != NULL)
    {
        status = StringCmp(fbp1->key, fbp2->key);
        if (status != 0)
            return status < 0;
    }

    return false;
}

/**********************************************************/
static bool SortFeaturesByOrder(const DataBlkPtr& sp1, const DataBlkPtr& sp2)
{
    FeatBlkPtr      fbp1;
    FeatBlkPtr      fbp2;

    fbp1 = (FeatBlkPtr) sp1->mpData;
    fbp2 = (FeatBlkPtr) sp2->mpData;

    return fbp1->num < fbp2->num;
}

/**********************************************************/
static DataBlkPtr fta_sort_features(DataBlkPtr dbp, bool order)
{
    DataBlkPtr* temp;
    DataBlkPtr      tdbp;
    Int4            total;
    Int4            i;

    for(total = 0, tdbp = dbp; tdbp != NULL; tdbp = tdbp->mpNext)
        total++;

    temp = (DataBlkPtr*) MemNew(total * sizeof(DataBlkPtr));

    for(i = 0, tdbp = dbp; tdbp != NULL; tdbp = tdbp->mpNext)
        temp[i++] = tdbp;

    std::sort(temp, temp + i, (order ? SortFeaturesByOrder : SortFeaturesByLoc));

    dbp = tdbp = temp[0];
    for(i = 0; i < total - 1; tdbp = tdbp->mpNext, i++)
        tdbp->mpNext = temp[i+1];

    tdbp = temp[total-1];
    tdbp->mpNext = NULL;

    MemFree(temp);

    return(dbp);
}

/**********************************************************/
static void fta_convert_to_regulatory(FeatBlkPtr fbp, const char *rclass)
{
    if(fbp == NULL || fbp->key == NULL || rclass == NULL)
        return;

    if(fbp->key != NULL)
        MemFree(fbp->key);
    fbp->key = StringSave("regulatory");

    CRef<objects::CGb_qual> qual(new objects::CGb_qual);
    qual->SetQual("regulatory_class");
    qual->SetVal(rclass);
    fbp->quals.push_back(qual);
}

/**********************************************************/
static void fta_check_replace_regulatory(DataBlkPtr dbp, unsigned char* drop)
{
    FeatBlkPtr fbp;
    const char **b;
    char*    p;
    bool       got_note;
    bool       other_class;
    Int4       count;
    Char       ch;

    for(; dbp != NULL; dbp = dbp->mpNext)
    {
        fbp = (FeatBlkPtr) dbp->mpData;
        if(fbp == NULL || fbp->key == NULL)
            continue;

        if(StringCmp(fbp->key, "attenuator") == 0)
            fta_convert_to_regulatory(fbp, "attenuator");
        else if(StringCmp(fbp->key, "CAAT_signal") == 0)
            fta_convert_to_regulatory(fbp, "CAAT_signal");
        else if(StringCmp(fbp->key, "enhancer") == 0)
            fta_convert_to_regulatory(fbp, "enhancer");
        else if(StringCmp(fbp->key, "GC_signal") == 0)
            fta_convert_to_regulatory(fbp, "GC_signal");
        else if(StringCmp(fbp->key, "-35_signal") == 0)
            fta_convert_to_regulatory(fbp, "minus_35_signal");
        else if(StringCmp(fbp->key, "-10_signal") == 0)
            fta_convert_to_regulatory(fbp, "minus_10_signal");
        else if(StringCmp(fbp->key, "polyA_signal") == 0)
            fta_convert_to_regulatory(fbp, "polyA_signal_sequence");
        else if(StringCmp(fbp->key, "promoter") == 0)
            fta_convert_to_regulatory(fbp, "promoter");
        else if(StringCmp(fbp->key, "RBS") == 0)
            fta_convert_to_regulatory(fbp, "ribosome_binding_site");
        else if(StringCmp(fbp->key, "TATA_signal") == 0)
            fta_convert_to_regulatory(fbp, "TATA_box");
        else if(StringCmp(fbp->key, "terminator") == 0)
            fta_convert_to_regulatory(fbp, "terminator");
        else if(StringCmp(fbp->key, "regulatory") != 0)
            continue;

        got_note = false;
        other_class = false;
        count = 0;

        ITERATE(TQualVector, cur, fbp->quals)
        {
            if (!(*cur)->IsSetQual() || !(*cur)->IsSetVal())
                continue;

            const std::string& qual_str = (*cur)->GetQual();

            if (qual_str != "regulatory_class")
            {
                if (qual_str == "note")
                    got_note = true;
                continue;
            }

            count++;
            if (!(*cur)->IsSetVal() || (*cur)->GetVal().empty())
            {
                ch = '\0';
                if(fbp->location == NULL || *fbp->location == '\0')
                    p = (char*) "(empty)";
                else
                {
                    p = fbp->location;
                    if(StringLen(p) > 50)
                    {
                        ch = p[50];
                        p[50] = '\0';
                    }
                }
                ErrPostEx(SEV_REJECT, ERR_QUALIFIER_InvalidRegulatoryClass,
                          "Empty /regulatory_class qualifier value in regulatory feature at location %s.",
                          p);
                if(ch != '\0')
                    p[50] = ch;
                *drop = 1;
                continue;
            }

            const std::string& val_str = (*cur)->GetVal();

            for (b = RegulatoryClassValues; *b != NULL; b++)
                if (val_str == *b)
                    break;

            if(*b != NULL)
            {
                if (val_str == "other")
                    other_class = true;
                continue;
            }

            ch = '\0';
            if(fbp->location == NULL || *fbp->location == '\0')
                p = (char*) "(empty)";
            else
            {
                p = fbp->location;
                if(StringLen(p) > 50)
                {
                    ch = p[50];
                    p[50] = '\0';
                }
            }
            ErrPostEx(SEV_REJECT, ERR_QUALIFIER_InvalidRegulatoryClass,
                      "Invalid /regulatory_class qualifier value %s provided in regulatory feature at location %s.",
                      val_str.c_str(), p);
            if(ch != '\0')
                p[50] = ch;
            *drop = 1;
        }

        if(count == 0)
        {
            ch = '\0';
            if(fbp->location == NULL || *fbp->location == '\0')
                p = (char*) "(empty)";
            else
            {
                p = fbp->location;
                if(StringLen(p) > 50)
                {
                    ch = p[50];
                    p[50] = '\0';
                }
            }
            ErrPostEx(SEV_REJECT, ERR_QUALIFIER_MissingRegulatoryClass,
                      "The regulatory feature is missing mandatory /regulatory_class qualifier at location %s.",
                      p);
            if(ch != '\0')
                p[50] = ch;
            *drop = 1;
        }
        else if(count > 1)
        {
            ch = '\0';
            if(fbp->location == NULL || *fbp->location == '\0')
                p = (char*) "(empty)";
            else
            {
                p = fbp->location;
                if(StringLen(p) > 50)
                {
                    ch = p[50];
                    p[50] = '\0';
                }
            }
            ErrPostEx(SEV_REJECT, ERR_QUALIFIER_MultipleRegulatoryClass,
                      "Multiple /regulatory_class qualifiers were encountered in regulatory feature at location %s.",
                      p);
            if(ch != '\0')
                p[50] = ch;
            *drop = 1;
        }

        if(other_class && !got_note)
        {
            ch = '\0';
            if(fbp->location == NULL || *fbp->location == '\0')
                p = (char*) "(empty)";
            else
            {
                p = fbp->location;
                if(StringLen(p) > 50)
                {
                    ch = p[50];
                    p[50] = '\0';
                }
            }
            ErrPostEx(SEV_REJECT, ERR_QUALIFIER_NoNoteForOtherRegulatory,
                      "The regulatory feature of class other is lacking required /note qualifier at location %s.",
                      p);
            if(ch != '\0')
                p[50] = ch;
            *drop = 1;
        }
    }
}

/**********************************************************/
static void fta_create_wgs_dbtag(objects::CBioseq &bioseq,
                                 const string& submitter_seqid,
                                 char* prefix, Int4 seqtype)
{
    char* dbname;

    dbname = (char*) MemNew(11);
    if(seqtype == 0 || seqtype == 1 || seqtype == 7)
        StringCpy(dbname, "WGS:");
    else if(seqtype == 4 || seqtype == 5 || seqtype == 8 || seqtype == 9)
        StringCpy(dbname, "TSA:");
    else
        StringCpy(dbname, "TLS:");
    StringCat(dbname, prefix);

    CRef<objects::CSeq_id> gen_id(new objects::CSeq_id);
    objects::CDbtag &tag = gen_id->SetGeneral();
    tag.SetTag().SetStr(submitter_seqid);
    tag.SetDb(dbname);
    bioseq.SetId().push_back(gen_id);
}

/**********************************************************/
static void fta_create_wgs_seqid(objects::CBioseq &bioseq,
                                 IndexblkPtr ibp, Parser::ESource source)
{
    TokenBlkPtr tbp;
    char*     prefix;
    char*     p;
    Int4        seqtype;
    Int4        i;

    if(!ibp ||  ibp->submitter_seqid.empty())
        return;

    prefix = NULL;

    seqtype = fta_if_wgs_acc(ibp->acnum);
    if(seqtype == 0 || seqtype == 3 || seqtype == 4 || seqtype == 6 ||
       seqtype == 10 || seqtype == 12)
    {
        ErrPostEx(SEV_REJECT, ERR_SOURCE_SubmitterSeqidNotAllowed,
                 "WGS/TLS/TSA master records are not allowed to have /submitter_seqid qualifiers, only contigs and scaffolds. Entry dropped.");
        ibp->drop = 1;
        return;
    }

    if(seqtype == 1 || seqtype == 5 || seqtype == 7 || seqtype == 8 ||
       seqtype == 9 || seqtype == 11)
    {
        prefix = StringSave(ibp->acnum);
        if(prefix[4] >= '0' && prefix[4] <= '9')
            prefix[6] = '\0';
        else
            prefix[8] = '\0';
        fta_create_wgs_dbtag(bioseq, ibp->submitter_seqid, prefix, seqtype);
        MemFree(prefix);
        return;
    }

    for(tbp = ibp->secaccs; tbp != NULL; tbp = tbp->next)
    {
        if(tbp->str[0] == '-')
            continue;

        if(prefix == NULL)
            prefix = StringSave(tbp->str);
        else
        {
            i = (prefix[4] >= '0' && prefix[4] <= '9') ? 6 : 8;
            if(StringNCmp(prefix, tbp->str, i) != 0)
                break;
        }
    }

    if(tbp == NULL && prefix != NULL)
    {
        seqtype = fta_if_wgs_acc(prefix);
        if(seqtype == 0 || seqtype == 1 || seqtype == 4 || seqtype == 5 ||
           seqtype == 7 || seqtype == 8 || seqtype == 9 || seqtype == 10 ||
           seqtype == 11)
        {
            if(prefix[4] >= '0' && prefix[4] <= '9')
                prefix[6] = '\0';
            else
                prefix[8] = '\0';
            fta_create_wgs_dbtag(bioseq, ibp->submitter_seqid, prefix,
                                 seqtype);
            MemFree(prefix);
            return;
        }
    }

    if(prefix != NULL)
    {
        MemFree(prefix);
        prefix = NULL;
    }

    if(bioseq.GetInst().IsSetExt() && bioseq.GetInst().GetExt().IsDelta())
    {
        objects::CDelta_ext::Tdata deltas =
            bioseq.GetInst().GetExt().GetDelta();
        objects::CDelta_ext::Tdata::iterator delta;

        for(delta = deltas.begin(); delta != deltas.end(); delta++)
        {
            const objects::CSeq_id *id = nullptr;

            if(!(*delta)->IsLoc())
                continue;

            const objects::CSeq_loc &locs = (*delta)->GetLoc();
            objects::CSeq_loc_CI ci(locs);

            for(; ci; ++ci)
            {
                CConstRef<objects::CSeq_loc> loc =
                    ci.GetRangeAsSeq_loc();
                if(!loc->IsInt())
                    continue;
                id = &ci.GetSeq_id();
                if(!id)
                    break;
                if(!id->IsGenbank() && !id->IsEmbl() && !id->IsDdbj() &&
                   !id->IsOther() && !id->IsTpg() && !id->IsTpe() &&
                   !id->IsTpd())
                    break;

                const objects::CTextseq_id *text_id =
                    id->GetTextseq_Id();
                if(text_id == nullptr || !text_id->IsSetAccession() ||
                   text_id->GetAccession().empty())
                    break;

                p = (char *) text_id->GetAccession().c_str();
                if(prefix == NULL)
                    prefix = StringSave(p);
                else
                {
                    i = (prefix[4] >= '0' && prefix[4] <= '9') ? 6 : 8;
                    if(StringNCmp(prefix, p, i) != 0)
                        break;
                }
            }
            if(ci)
                break;
        }

        if(delta == deltas.end() && prefix != NULL)
        {
            seqtype = fta_if_wgs_acc(prefix);
            if(seqtype == 0 || seqtype == 1 || seqtype == 4 || seqtype == 5 ||
               seqtype == 7 || seqtype == 8 || seqtype == 9 || seqtype == 10 ||
               seqtype == 11)
            {
                if(prefix[4] >= '0' && prefix[4] <= '9')
                    prefix[6] = '\0';
                else
                    prefix[8] = '\0';
                fta_create_wgs_dbtag(bioseq, ibp->submitter_seqid, prefix,
                                     seqtype);
                MemFree(prefix);
                return;
            }
        }

        if(prefix != NULL)
        {
             MemFree(prefix);
             prefix = NULL;
        }

        ErrPostEx(SEV_ERROR, ERR_SOURCE_SubmitterSeqidDropped,
                  "Could not determine project code for what appears to be a WGS/TLS/TSA scaffold record. /submitter_seqid dropped.");
        return;
    }

    if((source == Parser::ESource::EMBL || source == Parser::ESource::DDBJ) && ibp->is_tsa)
    {
        ErrPostEx(SEV_ERROR, ERR_SOURCE_SubmitterSeqidIgnored,
                  "Submitter sequence identifiers for non-project-based TSA records are not supported. /submitter_seqid \"%s\" has been dropped.",
                  ibp->submitter_seqid.c_str());
        return;
    }

    ErrPostEx(SEV_REJECT, ERR_SOURCE_SubmitterSeqidNotAllowed,
              "Only WGS/TLS/TSA related records (contigs and scaffolds) are allowed to have /submitter_seqid qualifier. This \"%s\" is not one of them. Entry dropped.",
              ibp->acnum);
    ibp->drop = 1;
}

/**********************************************************
 *
 *   SeqAnnotPtr LoadFeat(pp, entry, bsp):
 *
 *                                              5-4-93
 *
 **********************************************************/
void LoadFeat(ParserPtr pp, const DataBlk& entry, objects::CBioseq& bioseq)
{
    DataBlkPtr  dab;
    DataBlkPtr  dabnext;
    DataBlkPtr  dbp;
    DataBlkPtr  tdbp;
    FeatBlkPtr  fbp;

    IndexblkPtr ibp;
    Int4        col_data;
    Int2        type;
    Int4        i = 0;
    CRef<objects::CSeq_id> pat_seq_id;

    xinstall_gbparse_range_func(pp, flat2asn_range_func);

    ibp = pp->entrylist[pp->curindx];

    CRef<objects::CSeq_id> seq_id =
        MakeAccSeqId(ibp->acnum, pp->seqtype, pp->accver, ibp->vernum,
                     true, ibp->is_tpa);
    if(pp->source == Parser::ESource::USPTO)
    {
        pat_seq_id = new objects::CSeq_id;
        CRef<objects::CPatent_seq_id> pat_id = MakeUsptoPatSeqId(ibp->acnum);
        pat_seq_id->SetPatent(*pat_id);
    }

    if (!seq_id) {
        if (ibp->acnum && !NStr::IsBlank(ibp->acnum)) {
            seq_id = Ref(new CSeq_id(CSeq_id::e_Local, ibp->acnum));
        }
        else if (pp->mode == Parser::EMode::Relaxed) {
            seq_id = Ref(new CSeq_id(CSeq_id::e_Local, ibp->locusname));
        }
    }

    TSeqIdList ids;
    ids.push_back(seq_id);

    if(pp->format == Parser::EFormat::GenBank)
    {
        col_data = ParFlat_COL_DATA;
        type = ParFlat_FEATURES;
    }
    else if(pp->format == Parser::EFormat::XML)
    {
        col_data = 0;
        type = XML_FEATURES;
    }
    else
    {
        col_data = ParFlat_COL_DATA_EMBL;
        type = ParFlat_FH;
    }

    /* Find feature already isolated in a "block"
     * The key, location and qualifiers will be isolated to
     * a FeatBlk at the first step of ParseFeatureBlock, which
     * parses a single feature at a time.
     *                                          -Karl
     */
    if(pp->format == Parser::EFormat::XML)
        dab = XMLLoadFeatBlk(entry.mOffset, ibp->xip);
    else
        dab = TrackNodeType(entry, type);
    for(dbp = dab; dbp != NULL; dbp = dbp->mpNext)
    {
        if(dbp->mType != type)
            continue;

        /* Parsing each feature subblock to FeatBlkPtr, fbp
         * it also checks semantics of qualifiers and keys
         */
        if(pp->format == Parser::EFormat::XML)
            XMLParseFeatureBlock(pp->debug, (DataBlkPtr) dbp->mpData, pp->source);
        else
            ParseFeatureBlock(ibp, pp->debug, (DataBlkPtr) dbp->mpData, pp->source, pp->format);

        dbp->mpData = (DataBlkPtr) fta_sort_features((DataBlkPtr) dbp->mpData, false);
        fta_check_pseudogene_qual((DataBlkPtr) dbp->mpData);
        fta_check_old_locus_tags((DataBlkPtr) dbp->mpData, &ibp->drop);
        fta_check_compare_qual((DataBlkPtr) dbp->mpData, ibp->is_tpa);
        tdbp = (DataBlkPtr) dbp->mpData;
        for(i = 0; tdbp != NULL; i++, tdbp = tdbp->mpNext)
            fta_remove_dup_quals((FeatBlkPtr) tdbp->mpData);
        fta_remove_dup_feats((DataBlkPtr) dbp->mpData);
        for(tdbp = (DataBlkPtr) dbp->mpData; tdbp != NULL; tdbp = tdbp->mpNext)
            fta_check_rpt_unit_range((FeatBlkPtr) tdbp->mpData, ibp->bases);
        fta_check_multiple_locus_tag((DataBlkPtr) dbp->mpData, &ibp->drop);
        if(ibp->is_tpa || ibp->is_tsa || ibp->is_tls)
            fta_check_non_tpa_tsa_tls_locations((DataBlkPtr) dbp->mpData, ibp);
        fta_check_replace_regulatory((DataBlkPtr) dbp->mpData, &ibp->drop);
        dbp->mpData = fta_sort_features((DataBlkPtr) dbp->mpData, true);
    }

    if(i > 1 && ibp->is_mga)
    {
        ErrPostEx(SEV_REJECT, ERR_FEATURE_MoreThanOneCAGEFeat,
                  "CAGE records are allowed to have only one feature, and it must be the \"source\" one. Entry dropped.");
        ibp->drop = 1;
    }

    if(ibp->drop == 0)
        CollectGapFeats(entry, dab, pp, type);

    TSeqFeatList seq_feats;
    if(ibp->drop == 0)
        ParseSourceFeat(pp, dab, ids, type, bioseq, seq_feats);

    if (seq_feats.empty())
    {
        ibp->drop = 1;
        for(; dab != NULL; dab = dabnext)
        {
            dabnext = dab->mpNext;
            FreeFeatBlk((DataBlkPtr) dab->mpData, pp->format);
            if(pp->format == Parser::EFormat::XML)
                MemFree(dab);
        }
        xinstall_gbparse_range_func(NULL, NULL);
        return;
    }

    if(ibp->submitter_seqid != NULL)
        fta_create_wgs_seqid(bioseq, ibp, pp->source);

    objects::CSeq_descr::Tdata& descr_list = bioseq.SetDescr().Set();
    for (objects::CSeq_descr::Tdata::iterator descr = descr_list.begin(); descr != descr_list.end();)
    {
        if (!(*descr)->IsSource())
        {
            ++descr;
            continue;
        }

        descr = descr_list.erase(descr);
    }

    CRef<objects::CSeqdesc> descr_src(new objects::CSeqdesc);
    descr_src->SetSource(seq_feats.front()->SetData().SetBiosrc());

    descr_list.push_back(descr_src);
    seq_feats.pop_front();

    fta_get_gcode_from_biosource(descr_src->GetSource(), ibp);

    for(; dab != NULL; dab = dabnext)
    {
        dabnext = dab->mpNext;
        if(dab->mType != type)
        {
            if(pp->format == Parser::EFormat::XML)
                MemFree(dab);
            continue;
        }

        for(dbp = (DataBlkPtr) dab->mpData; dbp != NULL; dbp = dbp->mpNext)
        {
            if(dbp->mDrop == true)
                continue;

            fbp = (FeatBlkPtr) dbp->mpData;
            if(StringCmp(fbp->key, "source") == 0 ||
               StringCmp(fbp->key, "assembly_gap") == 0 ||
               (StringCmp(fbp->key, "gap") == 0 &&
                pp->source != Parser::ESource::DDBJ && pp->source != Parser::ESource::EMBL))
                continue;

            fta_sort_quals(fbp, pp->qamode);
            CRef<objects::CSeq_feat> feat = ProcFeatBlk(pp, fbp, ids);
            if (feat.Empty())
            {
                if(StringCmp(fbp->key, "CDS") == 0)
                {
                    ErrPostEx(SEV_ERROR, ERR_FEATURE_LocationParsing,
                              "CDS feature has unparsable location. Entry dropped. Location = [%s].",
                              fbp->location);
                    ibp->drop = 1;
                }
                continue;
            }

            if(StringCmp(fbp->key, "mobile_element") == 0 &&
               !fta_check_mobile_element(*feat))
            {
                ibp->drop = 1;
                continue;
            }

            fta_check_artificial_location(*feat, fbp->key);

            if(CheckForeignLoc(feat->GetLocation(),
               (pp->source == Parser::ESource::USPTO) ? *pat_seq_id : *seq_id))
            {
                ErrPostEx(SEV_WARNING, ERR_LOCATION_FailedCheck,
                          "Location pointing outside the entry [%s]",
                          fbp->location);

                if (feat->GetData().IsImp())
                {
                    const objects::CImp_feat& imp_feat = feat->GetData().GetImp();
                    if (imp_feat.GetKey() == "intron" ||
                        imp_feat.GetKey() == "exon")
                    {
                        /* foreign introns and exons wouldn't be parsed
                         */
                        feat.Reset();
                        continue;
                    }
                }
            }

            FilterDb_xref(*feat, pp->source);

            i = FTASeqLocCheck(feat->GetLocation(), ibp->acnum);
            if(i == 0)
            {
                ErrPostEx(SEV_WARNING, ERR_LOCATION_FailedCheck,
                          fbp->location);

                if(pp->debug)
                    seq_feats.push_back(feat);
                else
                {
                    feat.Reset();
                    continue;
                }
            }
            else
            {
                if(i == 1)
                {
                    if (feat->IsSetExcept_text() && feat->GetExcept_text() == "trans-splicing")
                        ErrPostEx(SEV_INFO,
                                  ERR_LOCATION_TransSpliceMixedStrand,
                                  "Mixed strands in SeqLoc of /trans_splicing feature: %s",
                                  fbp->location);
                    else
                        ErrPostEx(SEV_WARNING, ERR_LOCATION_MixedStrand,
                                  "Mixed strands in SeqLoc: %s", fbp->location);
                }

                seq_feats.push_back(feat);
            }
        }
        FreeFeatBlk((DataBlkPtr) dab->mpData, pp->format);
        if(pp->format == Parser::EFormat::XML)
            MemFree(dab);
    }

    if (!fta_perform_operon_checks(seq_feats, ibp))
    {
        ibp->drop = 1;
        seq_feats.clear();
        xinstall_gbparse_range_func(NULL, NULL);
        return;
    }

    bool stop = false;
    NON_CONST_ITERATE(TSeqFeatList, feat, seq_feats)
    {
        if (!(*feat)->GetData().IsImp())
            continue;

        const objects::CImp_feat& imp_feat = (*feat)->GetData().GetImp();

        if (imp_feat.IsSetKey() &&
            StringStr(imp_feat.GetKey().c_str(), "RNA") != NULL)
        {
            if (imp_feat.GetKey() == "ncRNA" && !fta_check_ncrna(*(*feat)))
            {
                stop = true;
                break;
            }

            GetRnaRef(*(*feat), bioseq, pp->source, pp->accver);
        }
    }

    if (stop)
    {
        ibp->drop = 1;
        seq_feats.clear();
        xinstall_gbparse_range_func(NULL, NULL);
        return;
    }

    SeqFeatPub(pp, entry, seq_feats, ids, col_data, ibp);
    if (seq_feats.empty() && ibp->drop != 0)
    {
        xinstall_gbparse_range_func(NULL, NULL);
        return;
    }

    /* ImpFeatPub() call will be removed in asn 4.0
     */
    ImpFeatPub(pp, entry, seq_feats, *seq_id, col_data, ibp);

    xinstall_gbparse_range_func(NULL, NULL);
    if (seq_feats.empty())
        return;

    CRef<objects::CSeq_annot> annot(new objects::CSeq_annot);
    annot->SetData().SetFtable().swap(seq_feats);

    bioseq.SetAnnot().push_back(annot);
}

/**********************************************************/
static Uint1 GetBiomolFromToks(char* mRNA, char* tRNA, char* rRNA,
                               char* snRNA, char* scRNA, char* uRNA,
                               char* snoRNA)
{
    char* p = NULL;

    if(mRNA != NULL)
        p = mRNA;
    if(p == NULL || (tRNA != NULL && tRNA < p))
        p = tRNA;
    if(p == NULL || (rRNA != NULL && rRNA < p))
        p = rRNA;
    if(p == NULL || (snRNA != NULL && snRNA < p))
        p = snRNA;
    if(p == NULL || (scRNA != NULL && scRNA < p))
        p = scRNA;
    if(p == NULL || (uRNA != NULL && uRNA < p))
        p = uRNA;
    if(p == NULL || (snoRNA != NULL && snoRNA < p))
        p = snoRNA;

    if(p == mRNA)
        return(Seq_descr_GIBB_mol_mRNA);
    if(p == tRNA)
        return(Seq_descr_GIBB_mol_tRNA);
    if(p == rRNA)
        return(Seq_descr_GIBB_mol_rRNA);
    if(p == snRNA || p == uRNA)
        return(Seq_descr_GIBB_mol_snRNA);
    if(p == snoRNA)
        return(Seq_descr_GIBB_mol_snoRNA);
    return(Seq_descr_GIBB_mol_scRNA);
}

/**********************************************************/
void GetFlatBiomol(int& biomol, Uint1 tech, char* molstr, ParserPtr pp,
                   const DataBlk& entry, const objects::COrg_ref* org_ref)
{
    Int4        genomic;
    char*     offset;
    Char        c;
    DataBlkPtr  dbp;

    Int2        count;
    Int2        i;
    EntryBlkPtr ebp;
    IndexblkPtr ibp;
    const char  *p;

    char*     q;
    char*     r;
    char*     mRNA = NULL;
    char*     tRNA = NULL;
    char*     rRNA = NULL;
    char*     snRNA = NULL;
    char*     scRNA = NULL;
    char*     uRNA = NULL;
    char*     snoRNA = NULL;
    bool        stage;
    bool        techok;
    bool        same;
    bool        is_syn;

    ebp = (EntryBlkPtr) entry.mpData;

    objects::CBioseq& bioseq = ebp->seq_entry->SetSeq();
    ibp = pp->entrylist[pp->curindx];

    if(ibp->is_prot)
    {
        bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_aa);
        biomol = 8;
        return;
    }

    if(StringCmp(ibp->division, "SYN") == 0 ||
       (org_ref != NULL && org_ref->IsSetOrgname() && org_ref->GetOrgname().IsSetDiv() &&
       org_ref->GetOrgname().GetDiv() == "SYN"))
        is_syn = true;
    else
        is_syn = false;

    r = NULL;
    c = '\0';
    if(ibp->moltype != NULL)
    {
        if(pp->source == Parser::ESource::DDBJ && StringNICmp(molstr, "PRT", 3) == 0)
            return;

        biomol = Seq_descr_GIBB_mol_genomic;
        bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_dna);

        if(molstr != NULL)
        {
            q = molstr;
            r = molstr;
            if(pp->format == Parser::EFormat::EMBL || pp->format == Parser::EFormat::XML)
                while(*r != ';' && *r != '\n' && *r != '\0')
                    r++;
            else
            {
                while(*r != ';' && *r != ' ' && *r != '\t' && *r != '\n' &&
                      *r != '\0')
                    r++;
                if(r - molstr > 10)
                    r = molstr + 10;
            }
            c = *r;
            *r = '\0';
            if(q == r)
                q = (char*) "???";
        }
        else
            q = (char*) "???";

        same = true;
        if(ibp->moltype == "genomic DNA")
        {
            biomol = Seq_descr_GIBB_mol_genomic;
            bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_dna);

            if(pp->source == Parser::ESource::EMBL)
            {
                if(NStr::CompareNocase(q, "DNA") != 0 &&
                    NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            }
            else if(NStr::CompareNocase(q, "DNA") != 0)
                same = false;
        }
        else if(ibp->moltype == "genomic RNA")
        {
            biomol = Seq_descr_GIBB_mol_genomic;
            bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_rna);

            if (pp->source == Parser::ESource::EMBL)
            {
                if(NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            }
            else if(NStr::CompareNocase(q, "RNA") != 0)
                same = false;
        }
        else if(ibp->moltype == "mRNA")
        {
            biomol = Seq_descr_GIBB_mol_mRNA;
            bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_rna);

            if(pp->source == Parser::ESource::EMBL)
            {
                if(NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            }
            else if(NStr::CompareNocase(q, "mRNA") != 0)
                same = false;
        }
        else if(ibp->moltype == "tRNA")
        {
            biomol = Seq_descr_GIBB_mol_tRNA;
            bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_rna);

            if(pp->source == Parser::ESource::EMBL)
            {
                if(NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            }
            else if(NStr::CompareNocase(q, "tRNA") != 0)
                same = false;
        }
        else if(ibp->moltype == "rRNA")
        {
            biomol = Seq_descr_GIBB_mol_rRNA;
            bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_rna);

            if(pp->source == Parser::ESource::EMBL)
            {
                if(NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            }
            else if(NStr::CompareNocase(q, "rRNA") != 0)
                same = false;
        }
        else if(ibp->moltype == "snoRNA")
        {
            biomol = Seq_descr_GIBB_mol_snoRNA;
            bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_rna);

            if(pp->source == Parser::ESource::EMBL)
            {
                if(NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            }
            else if(NStr::CompareNocase(q, "snoRNA") != 0)
                same = false;
        }
        else if(ibp->moltype == "snRNA")
        {
            biomol = Seq_descr_GIBB_mol_snRNA;
            bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_rna);

            if(pp->source == Parser::ESource::EMBL)
            {
                if(NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            }
            else if(NStr::CompareNocase(q, "snRNA") != 0)
                same = false;
        }
        else if(ibp->moltype == "scRNA")
        {
            biomol = Seq_descr_GIBB_mol_scRNA;
            bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_rna);

            if(pp->source == Parser::ESource::EMBL)
            {
                if(NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            }
            else if(NStr::CompareNocase(q, "scRNA") != 0)
                same = false;
        }
        else if(ibp->moltype == "pre-RNA")
        {
            biomol = Seq_descr_GIBB_mol_preRNA;
            bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_rna);

            if(pp->source == Parser::ESource::EMBL)
            {
                if(NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            }
            else if(NStr::CompareNocase(q, "RNA") != 0)
                same = false;
        }
        else if(ibp->moltype == "pre-mRNA")
        {
            biomol = Seq_descr_GIBB_mol_preRNA;
            bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_rna);

            if(pp->source == Parser::ESource::EMBL)
            {
                if(NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            }
            else if(NStr::CompareNocase(q, "RNA") != 0)
                same = false;
        }
        else if(ibp->moltype == "other RNA")
        {
            if(is_syn)
                biomol = Seq_descr_GIBB_mol_other_genetic;
            else
                biomol = Seq_descr_GIBB_mol_other;
            bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_rna);

            if (pp->source == Parser::ESource::EMBL)
            {
                if(NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            }
            else if(NStr::CompareNocase(q, "RNA") != 0)
                same = false;
        }
        else if(ibp->moltype == "other DNA")
        {
            if(is_syn)
                biomol = Seq_descr_GIBB_mol_other_genetic;
            else
                biomol = Seq_descr_GIBB_mol_other;
            bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_dna);

            if (pp->source == Parser::ESource::EMBL)
            {
                if(NStr::CompareNocase(q, "DNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            }
            else if(NStr::CompareNocase(q, "DNA") != 0)
                same = false;
        }
        else if(ibp->moltype == "unassigned RNA")
        {
            if(is_syn)
                biomol = Seq_descr_GIBB_mol_other_genetic;
            else
                biomol = Seq_descr_GIBB_mol_unknown;
            bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_rna);

            if (pp->source == Parser::ESource::EMBL)
            {
                if(NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            }
            else if(NStr::CompareNocase(q, "RNA") != 0)
                same = false;
        }
        else if(ibp->moltype == "unassigned DNA")
        {
            if(is_syn)
                biomol = Seq_descr_GIBB_mol_other_genetic;
            else
                biomol = Seq_descr_GIBB_mol_unknown;
            bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_dna);

            if (pp->source == Parser::ESource::EMBL)
            {
                if(NStr::CompareNocase(q, "DNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            }
            else if(NStr::CompareNocase(q, "DNA") != 0)
                same = false;
        }
        else if(ibp->moltype == "viral cRNA")
        {
            biomol = Seq_descr_GIBB_mol_cRNA;
            bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_rna);

            if (pp->source == Parser::ESource::EMBL)
            {
                if(NStr::CompareNocase(q, "RNA") != 0 &&
                   NStr::CompareNocase(q, "cRNA") != 0 &&
                   NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            }
            else if(NStr::CompareNocase(q, "cRNA") != 0)
                same = false;
        }
        else if(ibp->moltype == "transcribed RNA")
        {
            biomol = Seq_descr_GIBB_mol_trRNA;
            bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_rna);

            if (pp->source == Parser::ESource::EMBL)
            {
                if(NStr::CompareNocase(q, "RNA") != 0 && NStr::CompareNocase(ibp->moltype, q) != 0)
                    same = false;
            }
            else if(NStr::CompareNocase(q, "RNA") != 0)
                same = false;
        }
        else if(pp->source == Parser::ESource::USPTO &&
                NStr::CompareNocase(ibp->moltype, "protein") == 0)
        {
            biomol = 8;
            bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_aa);
        }
        else
        {
            ErrPostEx(SEV_REJECT, ERR_SOURCE_InvalidMolType,
                      "Invalid /mol_type value \"%s\" provided in source features. Entry dropped.",
                      ibp->moltype.c_str());
            ibp->drop = 1;
            if(molstr != NULL)
                *r = c;
            return;
        }

        if(!same)
        {
            if(ibp->embl_new_ID)
            {
                ErrPostEx(SEV_REJECT, ERR_SOURCE_MolTypesDisagree,
                          "Molecule type \"%s\" from the ID line disagrees with \"%s\" from the /mol_type qualifier.",
                          q, ibp->moltype.c_str());
                ibp->drop = 1;
                if(molstr != NULL)
                    *r = c;
                return;
            }
            ErrPostEx(SEV_ERROR, ERR_SOURCE_MolTypesDisagree,
                      "Molecule type \"%s\" from the ID/LOCUS line disagrees with \"%s\" from the /mol_type qualifier.",
                      q, ibp->moltype.c_str());
        }

        if ((tech == objects::CMolInfo::eTech_sts || tech == objects::CMolInfo::eTech_htgs_0 ||
            tech == objects::CMolInfo::eTech_htgs_1 || tech == objects::CMolInfo::eTech_htgs_2 ||
            tech == objects::CMolInfo::eTech_htgs_3 || tech == objects::CMolInfo::eTech_wgs ||
            tech == objects::CMolInfo::eTech_survey) &&
           ibp->moltype != "genomic DNA")
            techok = false;
        else if ((tech == objects::CMolInfo::eTech_est || tech == objects::CMolInfo::eTech_fli_cdna ||
            tech == objects::CMolInfo::eTech_htc) && ibp->moltype != "mRNA")
            techok = false;
        else
            techok = true;

        if(!techok)
        {
            if(tech == objects::CMolInfo::eTech_est)
                p = "EST";
            else if(tech == objects::CMolInfo::eTech_fli_cdna)
                p = "fli-cDNA";
            else if(tech == objects::CMolInfo::eTech_htc)
                p = "HTC";
            else if(tech == objects::CMolInfo::eTech_sts)
                p = "STS";
            else if(tech == objects::CMolInfo::eTech_wgs)
                p = "WGS";
            else if(tech == objects::CMolInfo::eTech_tsa)
                p = "TSA";
            else if(tech == objects::CMolInfo::eTech_targeted)
                p = "TLS";
            else if(tech == objects::CMolInfo::eTech_survey)
                p = "GSS";
            else
                p = "HTG";
            ErrPostEx(SEV_ERROR, ERR_SOURCE_MolTypeSeqTypeConflict,
                      "Molecule type \"%s\" from the /mol_type qualifier disagrees with this record's sequence type: \"%s\".",
                      ibp->moltype.c_str(), p);
        }

        if(molstr != NULL)
            *r = c;
        return;
    }

    if(tech == objects::CMolInfo::eTech_est)
    {
        biomol = Seq_descr_GIBB_mol_mRNA;
        bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_rna);
        return;
    }

    if(pp->source == Parser::ESource::DDBJ || pp->source == Parser::ESource::LANL ||
       pp->source == Parser::ESource::NCBI)
    {
        biomol = Seq_descr_GIBB_mol_genomic;
        bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_dna);
    }
    else
    {
        biomol = Unknown;
        bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_na);
    }

    if(molstr == NULL)
        genomic = -1;
    else
    {
        genomic = CheckNA(molstr);
        if(genomic < 0 && pp->source == Parser::ESource::DDBJ)
            genomic = CheckNADDBJ(molstr);
    }

    if(genomic < 0 || genomic > 20)
    {
        if(pp->source == Parser::ESource::EMBL && StringNICmp(molstr, "XXX", 3) == 0)
            return;
        if(pp->source == Parser::ESource::DDBJ && StringNICmp(molstr, "PRT", 3) == 0)
            return;
        ibp->drop = 1;
        q = molstr;
        c = '\0';
        if(q != NULL)
        {
            if(pp->format == Parser::EFormat::EMBL)
                while(*q != ';' && *q != '\n' && *q != '\0')
                    q++;
            else
            {
                while(*q != ';' && *q != ' ' && *q != '\t' && *q != '\n' &&
                      *q != '\0')
                    q++;
                if(q - molstr > 10)
                    q = molstr + 10;
            }

            c = *q;
            *q = '\0';
        }
        if(pp->source == Parser::ESource::DDBJ)
            p = "DDBJ";
        else if(pp->source == Parser::ESource::EMBL)
            p = "EMBL";
        else if(pp->source == Parser::ESource::LANL)
            p = "LANL";
        else
            p = "NCBI";

        ErrPostEx(SEV_FATAL, ERR_FORMAT_InvalidMolType,
                  "Molecule type \"%s\" from LOCUS/ID line is not legal value for records from source \"%s\". Sequence rejected.",
                  (molstr == NULL) ? "???" : molstr, p);
        if(q != NULL)
            *q = c;
        return;
    }

    if(genomic < 2)
        bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_na);
    else if(genomic > 1 && genomic < 6)
        bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_dna);
    else
        bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_rna);

    if(genomic != 6)                    /* Not just RNA */
    {
        if(genomic < 2)                 /* "   ", "NA" or "cDNA" */
            biomol = Seq_descr_GIBB_mol_genomic;
        else if(genomic == 2)                   /* DNA */
            biomol = Seq_descr_GIBB_mol_genomic;
        else if(genomic == 3)                   /* genomic DNA */
            biomol = Seq_descr_GIBB_mol_genomic;
        else if(genomic == 4)                   /* other DNA */
        {
            if(is_syn)
                biomol = Seq_descr_GIBB_mol_other_genetic;
            else
                biomol = Seq_descr_GIBB_mol_other;
        }
        else if(genomic == 5)                   /* unassigned DNA */
        {
            if(is_syn)
                biomol = Seq_descr_GIBB_mol_other_genetic;
            else
                biomol = Seq_descr_GIBB_mol_unknown;
        }
        else if(genomic == 7)                   /* mRNA */
            biomol = Seq_descr_GIBB_mol_mRNA;
        else if(genomic == 8)                   /* rRNA */
            biomol = Seq_descr_GIBB_mol_rRNA;
        else if(genomic == 9)                   /* tRNA */
            biomol = Seq_descr_GIBB_mol_tRNA;
        else if(genomic == 10 || genomic == 12) /* uRNA -> snRNA */
            biomol = Seq_descr_GIBB_mol_snRNA;
        else if(genomic == 11)                  /* scRNA */
            biomol = Seq_descr_GIBB_mol_scRNA;
        else if(genomic == 13)                  /* snoRNA */
            biomol = Seq_descr_GIBB_mol_snoRNA;
        else if(genomic == 14)                  /* pre-RNA */
            biomol = Seq_descr_GIBB_mol_preRNA;
        else if(genomic == 15)                  /* pre-mRNA */
            biomol = Seq_descr_GIBB_mol_preRNA;
        else if(genomic == 16)                  /* genomic RNA */
            biomol = Seq_descr_GIBB_mol_genomic;
        else if(genomic == 17)                  /* other RNA */
        {
            if(is_syn)
                biomol = Seq_descr_GIBB_mol_other_genetic;
            else
                biomol = Seq_descr_GIBB_mol_other;
         }
        else if(genomic == 18)                  /* unassigned RNA */
        {
            if(is_syn)
                biomol = Seq_descr_GIBB_mol_other_genetic;
            else
                biomol = Seq_descr_GIBB_mol_unknown;
        }
        else if(genomic == 19 || genomic == 20) /* cRNA or viral cRNA */
            biomol = Seq_descr_GIBB_mol_cRNA;
        return;
    }

    /* Here goes most complicated case with just RNA
     */
    const Char* div = NULL;
    if (org_ref != NULL && org_ref->IsSetOrgname() && org_ref->GetOrgname().IsSetDiv())
        div = org_ref->GetOrgname().GetDiv().c_str();

    if(pp->source != Parser::ESource::EMBL || pp->format != Parser::EFormat::EMBL)
    {
        biomol = Seq_descr_GIBB_mol_genomic;
        if (div == NULL || StringNCmp(div, "VRL", 3) != 0)
        {
            ErrPostEx(SEV_ERROR, ERR_LOCUS_NonViralRNAMoltype,
                      "Genomic RNA implied by presence of RNA moltype, but sequence is non-viral.");
        }
        return;
    }

    count = 0;
    size_t len = 0;
    offset = xSrchNodeType(entry, ParFlat_DE, &len);
    if(offset != NULL)
    {
        c = offset[len];
        offset[len] = '\0';
        mRNA = StringStr(offset, "mRNA");
        tRNA = StringStr(offset, "tRNA");
        rRNA = StringStr(offset, "rRNA");
        snRNA = StringStr(offset, "snRNA");
        scRNA = StringStr(offset, "scRNA");
        uRNA = StringStr(offset, "uRNA");
        snoRNA = StringStr(offset, "snoRNA");
        if(mRNA != NULL)
            count++;
        if(tRNA != NULL)
            count++;
        if(rRNA != NULL)
            count++;
        if(snRNA != NULL || uRNA != NULL)
            count++;
        if(scRNA != NULL)
            count++;
        if(snoRNA != NULL)
            count++;
        offset[len] = c;
    }

    /* Non-viral division
     */
    if (div == NULL || StringNCmp(div, "VRL", 3) != 0)
    {
        biomol = Seq_descr_GIBB_mol_mRNA;

        if(count > 1)
        {
            ErrPostEx(SEV_WARNING, ERR_DEFINITION_DifferingRnaTokens,
                      "More than one of mRNA, tRNA, rRNA, snRNA (uRNA), scRNA, snoRNA present in defline.");
        }

        if(tRNA != NULL)
        {
            for(p = tRNA + 4; *p == ' ' || *p == '\t';)
                p++;
            if(*p == '\n')
            {
                p++;
                if(StringNCmp(p, "DE   ", 5) == 0)
                    p += 5;
            }
            if(StringNICmp(p, "Synthetase", 10) == 0)
                return;
        }

        if(count > 0)
            biomol = GetBiomolFromToks(mRNA, tRNA, rRNA, snRNA, scRNA, uRNA,
                                       snoRNA);
        return;
    }

    /* Viral division
     */
    if (org_ref != NULL && org_ref->IsSetOrgname() && org_ref->GetOrgname().IsSetLineage() &&
        StringIStr(org_ref->GetOrgname().GetLineage().c_str(), "no DNA stage") != NULL)
         stage = true;
    else
         stage = false;

    dbp = TrackNodeType(entry, ParFlat_FH);
    if(dbp == NULL)
        return;
    dbp = (DataBlkPtr) dbp->mpData;
    for(i = 0; dbp != NULL && i < 2; dbp = dbp->mpNext)
    {
        if(dbp->mOffset == NULL)
            continue;
        offset = dbp->mOffset + ParFlat_COL_FEATKEY;
        if(StringNCmp(offset, "CDS", 3) == 0)
            i++;
    }
    if(i > 1)
    {
        biomol = Seq_descr_GIBB_mol_genomic;
        if(!stage)
        {
            ErrPostEx(SEV_WARNING, ERR_SOURCE_GenomicViralRnaAssumed,
                      "This sequence is assumed to be genomic due to multiple coding region but lack of a DNA stage is not indicated in taxonomic lineage.");
        }
        return;
    }

    if(count == 0)
    {
        biomol = Seq_descr_GIBB_mol_genomic;
        if(!stage)
        {
            ErrPostEx(SEV_ERROR, ERR_SOURCE_UnclassifiedViralRna,
                      "Cannot determine viral molecule type (genomic vs a specific type of RNA) based on definition line, CDS content, or taxonomic lineage. So this sequence has been classified as genomic by default (perhaps in error).");
        }
        else
        {
            ErrPostEx(SEV_WARNING, ERR_SOURCE_LineageImpliesGenomicViralRna,
                      "This sequence lacks indication of specific RNA type in the definition line, but the taxonomic lineage mentions lack of a DNA stage, so it is classified as genomic.");
        }
        return;
    }

    if(count > 1)
    {
        ErrPostEx(SEV_WARNING, ERR_DEFINITION_DifferingRnaTokens,
                  "More than one of mRNA, tRNA, rRNA, snRNA (uRNA), scRNA, snoRNA present in defline.");
    }

    biomol = GetBiomolFromToks(mRNA, tRNA, rRNA, snRNA, scRNA, uRNA, snoRNA);
}

END_NCBI_SCOPE
