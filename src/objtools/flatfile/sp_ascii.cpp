/* sp_ascii.c
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
 * File Name:  sp_ascii.c
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *      Build SWISS-PROT format entry block. All external variables
 * are in sp_global.c.
 *      Parse SP image in memory to asn.
 *
 */
#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objects/seqblock/SP_block.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seqblock/PIR_block.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seq/Pubdesc.hpp>

#include "index.h"
#include "sprot.h"

#include <objtools/flatfile/flatdefn.h>
#include "ftanet.h"
#include <objtools/flatfile/flatfile_parser.hpp>

#include "ftaerr.hpp"
#include "indx_blk.h"
#include "asci_blk.h"
#include "sp_ascii.h"
#include "utilfeat.h"
#include "add.h"
#include "nucprot.h"
#include "utilfun.h"
#include "entry.h"
#include "ref.h"
#include "xutils.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "sp_ascii.cpp"

BEGIN_NCBI_SCOPE

const char *ParFlat_SPComTopics[] = {
    "ALLERGEN:",
    "ALTERNATIVE PRODUCTS:",
    "BIOPHYSICOCHEMICAL PROPERTIES:",
    "BIOTECHNOLOGY:",
    "CATALYTIC ACTIVITY:",
    "CAUTION:",
    "COFACTOR:",
    "DATABASE:",
    "DEVELOPMENTAL STAGE:",
    "DISEASE:",
    "DISRUPTION PHENOTYPE:",
    "DOMAIN:",
    "ENZYME REGULATION:",
    "FUNCTION:",
    "INDUCTION:",
    "INTERACTION:",
    "MASS SPECTROMETRY:",
    "MISCELLANEOUS:",
    "PATHWAY:",
    "PHARMACEUTICAL:",
    "POLYMORPHISM:",
    "PTM:",
    "RNA EDITING:",
    "SEQUENCE CAUTION:",
    "SIMILARITY:",
    "SUBCELLULAR LOCATION:",
    "SUBUNIT:",
    "TISSUE SPECIFICITY:",
    "TOXIC DOSE:",
    "WEB RESOURCE:",
    NULL
};

/* bsv : 03/04/2020 : no Seq-feat.exp-ev setting anymore
const char *ParFlat_SPFeatNoExp[] = {
    "(PROBABLE).",
    "(PROBABLE)",
    "PROBABLE.",
    "(POTENTIAL).",
    "(POTENTIAL)",
    "POTENTIAL.",
    "(BY SIMILARITY).",
    "(BY SIMILARITY)",
    "BY SIMILARITY.",
    NULL
};

const char *ParFlat_SPFeatNoExpW[] = {
    "(PUTATIVE).",
    "(PUTATIVE)",
    "PUTATIVE.",
    "(SIMILARITY).",
    "(SIMILARITY)",
    "SIMILARITY.",
    "(POSSIBLE).",
    "(POSSIBLE)",
    "POSSIBLE.",
    "(POSTULATED).",
    "(POSTULATED)",
    "POSTULATED.",
    "(BY HOMOLOGY).",
    "(BY HOMOLOGY)",
    "BY HOMOLOGY.",
    NULL
};
*/

SPFeatType ParFlat_SPFeat[] = {
    { "ACT_SITE", ParFlatSPSites, 1, NULL },
    { "BINDING", ParFlatSPSites, 2, NULL },
    { "CARBOHYD", ParFlatSPSites, 6, NULL },
    { "MUTAGEN", ParFlatSPSites, 8, NULL },
    { "METAL", ParFlatSPSites, 9, NULL },
    { "LIPID", ParFlatSPSites, 20, NULL },
    { "NP_BIND", ParFlatSPSites, 21, NULL },
    { "DNA_BIND", ParFlatSPSites, 22, NULL },
    { "SITE", ParFlatSPSites, 255, NULL },
    { "MOD_RES", ParFlatSPSites, 5, NULL },  /* 9 */
    { "MOD_RES", ParFlatSPSites, 10, "4-aspartylphosphate" },
    { "MOD_RES", ParFlatSPSites, 10, "5-glutamyl glycerylphosphorylethanolamine" },
    { "MOD_RES", ParFlatSPSites, 10, "Phosphoarginine" },
    { "MOD_RES", ParFlatSPSites, 10, "Phosphocysteine" },
    { "MOD_RES", ParFlatSPSites, 10, "Phosphohistidine" },
    { "MOD_RES", ParFlatSPSites, 10, "PHOSPHORYLATION" },
    { "MOD_RES", ParFlatSPSites, 10, "Phosphoserine" },
    { "MOD_RES", ParFlatSPSites, 10, "Phosphothreonine" },
    { "MOD_RES", ParFlatSPSites, 10, "Phosphotyrosine" },
    { "MOD_RES", ParFlatSPSites, 10, "Pros-phosphohistidine" },
    { "MOD_RES", ParFlatSPSites, 10, "Tele-phosphohistidine" },
    { "MOD_RES", ParFlatSPSites, 11, "ACETYLATION" },
    { "MOD_RES", ParFlatSPSites, 11, "N2-acetylarginine" },
    { "MOD_RES", ParFlatSPSites, 11, "N6-acetyllysine" },
    { "MOD_RES", ParFlatSPSites, 11, "N-acetylalanine" },
    { "MOD_RES", ParFlatSPSites, 11, "N-acetylaspartate" },
    { "MOD_RES", ParFlatSPSites, 11, "N-acetylated lysine" },
    { "MOD_RES", ParFlatSPSites, 11, "N-acetylcysteine" },
    { "MOD_RES", ParFlatSPSites, 11, "N-acetylglutamate" },
    { "MOD_RES", ParFlatSPSites, 11, "N-acetylglycine" },
    { "MOD_RES", ParFlatSPSites, 11, "N-acetylisoleucine" },
    { "MOD_RES", ParFlatSPSites, 11, "N-acetylmethionine" },
    { "MOD_RES", ParFlatSPSites, 11, "N-acetylproline" },
    { "MOD_RES", ParFlatSPSites, 11, "N-acetylserine" },
    { "MOD_RES", ParFlatSPSites, 11, "N-acetylthreonine" },
    { "MOD_RES", ParFlatSPSites, 11, "N-acetyltyrosine" },
    { "MOD_RES", ParFlatSPSites, 11, "N-acetylvaline" },
    { "MOD_RES", ParFlatSPSites, 11, "O-acetylserine" },
    { "MOD_RES", ParFlatSPSites, 11, "O-acetylthreonine" },
    { "MOD_RES", ParFlatSPSites, 12, "Alanine amide" },
    { "MOD_RES", ParFlatSPSites, 12, "AMIDATION" },
    { "MOD_RES", ParFlatSPSites, 12, "Arginine amide" },
    { "MOD_RES", ParFlatSPSites, 12, "Asparagine amide" },
    { "MOD_RES", ParFlatSPSites, 12, "Aspartic acid 1-amide" },
    { "MOD_RES", ParFlatSPSites, 12, "Cysteine amide" },
    { "MOD_RES", ParFlatSPSites, 12, "Glutamic acid 1-amide" },
    { "MOD_RES", ParFlatSPSites, 12, "Glutamine amide" },
    { "MOD_RES", ParFlatSPSites, 12, "Glycine amide" },
    { "MOD_RES", ParFlatSPSites, 12, "Histidine amide" },
    { "MOD_RES", ParFlatSPSites, 12, "Isoleucine amide" },
    { "MOD_RES", ParFlatSPSites, 12, "Leucine amide" },
    { "MOD_RES", ParFlatSPSites, 12, "Lysine amide" },
    { "MOD_RES", ParFlatSPSites, 12, "Methionine amide" },
    { "MOD_RES", ParFlatSPSites, 12, "Phenylalanine amide" },
    { "MOD_RES", ParFlatSPSites, 12, "Proline amide" },
    { "MOD_RES", ParFlatSPSites, 12, "Serine amide" },
    { "MOD_RES", ParFlatSPSites, 12, "Threonine amide" },
    { "MOD_RES", ParFlatSPSites, 12, "Tryptophan amide" },
    { "MOD_RES", ParFlatSPSites, 12, "Tyrosine amide" },
    { "MOD_RES", ParFlatSPSites, 12, "Valine amide" },
    { "MOD_RES", ParFlatSPSites, 13, "2-methylglutamine" },
    { "MOD_RES", ParFlatSPSites, 13, "2'-methylsulfonyltryptophan" },
    { "MOD_RES", ParFlatSPSites, 13, "3-methylthioaspartic acid" },
    { "MOD_RES", ParFlatSPSites, 13, "5-methylarginine" },
    { "MOD_RES", ParFlatSPSites, 13, "Asymmetric dimethylarginine" },
    { "MOD_RES", ParFlatSPSites, 13, "Cysteine methyl disulfide" },
    { "MOD_RES", ParFlatSPSites, 13, "Cysteine methyl ester" },
    { "MOD_RES", ParFlatSPSites, 13, "Dimethylated arginine" },
    { "MOD_RES", ParFlatSPSites, 13, "Glutamate methyl ester (Gln)" },
    { "MOD_RES", ParFlatSPSites, 13, "Glutamate methyl ester (Glu)" },
    { "MOD_RES", ParFlatSPSites, 13, "Leucine methyl ester" },
    { "MOD_RES", ParFlatSPSites, 13, "Lysine methyl ester" },
    { "MOD_RES", ParFlatSPSites, 13, "METHYLATION" },
    { "MOD_RES", ParFlatSPSites, 13, "Methylhistidine" },
    { "MOD_RES", ParFlatSPSites, 13, "N,N,N-trimethylalanine" },
    { "MOD_RES", ParFlatSPSites, 13, "N,N,N-trimethylglycine" },
    { "MOD_RES", ParFlatSPSites, 13, "N,N,N-trimethylserine" },
    { "MOD_RES", ParFlatSPSites, 13, "N,N-dimethylalanine" },
    { "MOD_RES", ParFlatSPSites, 13, "N,N-dimethylglycine" },
    { "MOD_RES", ParFlatSPSites, 13, "N,N-dimethylleucine" },
    { "MOD_RES", ParFlatSPSites, 13, "N,N-dimethylproline" },
    { "MOD_RES", ParFlatSPSites, 13, "N,N-dimethylserine" },
    { "MOD_RES", ParFlatSPSites, 13, "N2,N2-dimethylarginine" },
    { "MOD_RES", ParFlatSPSites, 13, "N4,N4-dimethylasparagine" },
    { "MOD_RES", ParFlatSPSites, 13, "N4-methylasparagine" },
    { "MOD_RES", ParFlatSPSites, 13, "N5-methylarginine" },
    { "MOD_RES", ParFlatSPSites, 13, "N5-methylglutamine" },
    { "MOD_RES", ParFlatSPSites, 13, "N6,N6,N6-trimethyl-5-hydroxylysine" },
    { "MOD_RES", ParFlatSPSites, 13, "N6,N6,N6-trimethyllysine" },
    { "MOD_RES", ParFlatSPSites, 13, "N6,N6-dimethyllysine" },
    { "MOD_RES", ParFlatSPSites, 13, "N6-methylated lysine" },
    { "MOD_RES", ParFlatSPSites, 13, "N6-methyllysine" },
    { "MOD_RES", ParFlatSPSites, 13, "N6-poly(methylaminopropyl)lysine" },
    { "MOD_RES", ParFlatSPSites, 13, "N-methylalanine" },
    { "MOD_RES", ParFlatSPSites, 13, "N-methylglycine" },
    { "MOD_RES", ParFlatSPSites, 13, "N-methylisoleucine" },
    { "MOD_RES", ParFlatSPSites, 13, "N-methylleucine" },
    { "MOD_RES", ParFlatSPSites, 13, "N-methylmethionine" },
    { "MOD_RES", ParFlatSPSites, 13, "N-methylphenylalanine" },
    { "MOD_RES", ParFlatSPSites, 13, "N-methylproline" },
    { "MOD_RES", ParFlatSPSites, 13, "N-methylserine" },
    { "MOD_RES", ParFlatSPSites, 13, "N-methyltyrosine" },
    { "MOD_RES", ParFlatSPSites, 13, "Omega-N-methylarginine" },
    { "MOD_RES", ParFlatSPSites, 13, "Omega-N-methylated arginine" },
    { "MOD_RES", ParFlatSPSites, 13, "O-methylthreonine" },
    { "MOD_RES", ParFlatSPSites, 13, "Pros-methylhistidine" },
    { "MOD_RES", ParFlatSPSites, 13, "S-methylcysteine" },
    { "MOD_RES", ParFlatSPSites, 13, "Symmetric dimethylarginine" },
    { "MOD_RES", ParFlatSPSites, 13, "Tele-methylhistidine" },
    { "MOD_RES", ParFlatSPSites, 13, "Threonine methyl ester" },
    { "MOD_RES", ParFlatSPSites, 14, "(3R)-3-hydroxyarginine" },
    { "MOD_RES", ParFlatSPSites, 14, "(3R)-3-hydroxyasparagine" },
    { "MOD_RES", ParFlatSPSites, 14, "(3R)-3-hydroxyaspartate" },
    { "MOD_RES", ParFlatSPSites, 14, "(3S)-3-hydroxyhistidine" },
    { "MOD_RES", ParFlatSPSites, 14, "(3R,4R)-3,4-dihydroxyproline" },
    { "MOD_RES", ParFlatSPSites, 14, "(3R,4R)-4,5-dihydroxyisoleucine" },
    { "MOD_RES", ParFlatSPSites, 14, "(3R,4S)-3,4-dihydroxyproline" },
    { "MOD_RES", ParFlatSPSites, 14, "(3R,4S)-4-hydroxyisoleucine" },
    { "MOD_RES", ParFlatSPSites, 14, "(3S)-3-hydroxyasparagine" },
    { "MOD_RES", ParFlatSPSites, 14, "(3S)-3-hydroxyaspartate" },
    { "MOD_RES", ParFlatSPSites, 14, "(3S,4R)-3,4-dihydroxyisoleucine" },
    { "MOD_RES", ParFlatSPSites, 14, "(4R)-5-hydroxyleucine" },
    { "MOD_RES", ParFlatSPSites, 14, "(4R)-4,5-dihydroxyleucine" },
    { "MOD_RES", ParFlatSPSites, 14, "3,4-dihydroxyarginine" },
    { "MOD_RES", ParFlatSPSites, 14, "3',4'-dihydroxyphenylalanine" },
    { "MOD_RES", ParFlatSPSites, 14, "3,4-dihydroxyproline" },
    { "MOD_RES", ParFlatSPSites, 14, "3-hydroxyasparagine" },
    { "MOD_RES", ParFlatSPSites, 14, "3-hydroxyaspartate" },
    { "MOD_RES", ParFlatSPSites, 14, "3-hydroxyphenylalanine" },
    { "MOD_RES", ParFlatSPSites, 14, "3-hydroxyproline" },
    { "MOD_RES", ParFlatSPSites, 14, "3-hydroxytryptophan" },
    { "MOD_RES", ParFlatSPSites, 14, "3-hydroxyvaline" },
    { "MOD_RES", ParFlatSPSites, 14, "4,5,5'-trihydroxyleucine" },
    { "MOD_RES", ParFlatSPSites, 14, "4,5-dihydroxylysine" },
    { "MOD_RES", ParFlatSPSites, 14, "4-hydroxyarginine" },
    { "MOD_RES", ParFlatSPSites, 14, "4-hydroxyglutamate" },
    { "MOD_RES", ParFlatSPSites, 14, "4-hydroxyproline" },
    { "MOD_RES", ParFlatSPSites, 14, "5-hydroxy-3-methylproline (Ile)" },
    { "MOD_RES", ParFlatSPSites, 14, "5-hydroxylysine" },
    { "MOD_RES", ParFlatSPSites, 14, "(5R)-5-hydroxylysine" },
    { "MOD_RES", ParFlatSPSites, 14, "(5S)-5-hydroxylysine" },
    { "MOD_RES", ParFlatSPSites, 14, "7'-hydroxytryptophan" },
    { "MOD_RES", ParFlatSPSites, 14, "D-4-hydroxyvaline" },
    { "MOD_RES", ParFlatSPSites, 14, "HYDROXYLATION" },
    { "MOD_RES", ParFlatSPSites, 14, "Hydroxyproline" },
    { "MOD_RES", ParFlatSPSites, 14, "N6-(3,6-diaminohexanoyl)-5-hydroxylysine" },
    { "MOD_RES", ParFlatSPSites, 15, "SULFATATION" },
    { "MOD_RES", ParFlatSPSites, 15, "Sulfoserine"},
    { "MOD_RES", ParFlatSPSites, 15, "Sulfothreonine"},
    { "MOD_RES", ParFlatSPSites, 15, "Sulfotyrosine"},
    { "MOD_RES", ParFlatSPSites, 16, "OXIDATIVE DEAMINATION" },
    { "MOD_RES", ParFlatSPSites, 17, "Pyrrolidone carboxylic acid" },
    { "MOD_RES", ParFlatSPSites, 17, "Pyrrolidone carboxylic acid (Glu)" },
    { "MOD_RES", ParFlatSPSites, 18, "4-carboxyglutamate" },
    { "MOD_RES", ParFlatSPSites, 18, "GAMMA-CARBOXYGLUTAMIC ACID" },
    { "MOD_RES", ParFlatSPSites, 19, "Blocked" },
    { "MOD_RES", ParFlatSPSites, 19, "Blocked amino end (Ala)" },
    { "MOD_RES", ParFlatSPSites, 19, "Blocked amino end (Arg)" },
    { "MOD_RES", ParFlatSPSites, 19, "Blocked amino end (Asn)" },
    { "MOD_RES", ParFlatSPSites, 19, "Blocked amino end (Asp)" },
    { "MOD_RES", ParFlatSPSites, 19, "Blocked amino end (Asx)" },
    { "MOD_RES", ParFlatSPSites, 19, "Blocked amino end (Cys)" },
    { "MOD_RES", ParFlatSPSites, 19, "Blocked amino end (Gln)" },
    { "MOD_RES", ParFlatSPSites, 19, "Blocked amino end (Glu)" },
    { "MOD_RES", ParFlatSPSites, 19, "Blocked amino end (Gly)" },
    { "MOD_RES", ParFlatSPSites, 19, "Blocked amino end (Ile)" },
    { "MOD_RES", ParFlatSPSites, 19, "Blocked amino end (Leu)" },
    { "MOD_RES", ParFlatSPSites, 19, "Blocked amino end (Met)" },
    { "MOD_RES", ParFlatSPSites, 19, "Blocked amino end (Pro)" },
    { "MOD_RES", ParFlatSPSites, 19, "Blocked amino end (Ser)" },
    { "MOD_RES", ParFlatSPSites, 19, "Blocked amino end (Thr)" },
    { "MOD_RES", ParFlatSPSites, 19, "Blocked amino end (Val)" },
    { "MOD_RES", ParFlatSPSites, 19, "Blocked amino end (Xaa)" },
    { "MOD_RES", ParFlatSPSites, 19, "Blocked carboxyl end (Arg)" },
    { "MOD_RES", ParFlatSPSites, 19, "Blocked carboxyl end (His)" },  /* 174 */
    { "DISULFID", ParFlatSPBonds, 1, NULL },
    { "THIOLEST", ParFlatSPBonds, 2, NULL },
    { "CROSSLNK", ParFlatSPBonds, 3, NULL },
    { "THIOETH", ParFlatSPBonds, 4, NULL },
    { "SIGNAL", ParFlatSPRegions, -1, "Signal" },
    { "PROPEP", ParFlatSPRegions, -1, "Propeptide" },
    { "CHAIN", ParFlatSPRegions, -1, "Mature chain" },
    { "TRANSIT", ParFlatSPRegions, -1, "Transit peptide" },
    { "PEPTIDE", ParFlatSPRegions, -1, "Processed active peptide" },
    { "DOMAIN", ParFlatSPRegions, -1, "Domain" },
    { "CA_BIND", ParFlatSPRegions, -1, "Calcium binding region" },
    { "TRANSMEM", ParFlatSPRegions, -1, "Transmembrane region" },
    { "ZN_FING", ParFlatSPRegions, -1, "Zinc finger region" },
    { "SIMILAR", ParFlatSPRegions, -1, "Similarity" },
    { "REPEAT", ParFlatSPRegions, -1, "Repetitive region" },
    { "HELIX", ParFlatSPRegions, -1, "Helical region" },
    { "STRAND", ParFlatSPRegions, -1, "Beta-strand region" },
    { "TURN", ParFlatSPRegions, -1, "Hydrogen bonded turn" },
    { "CONFLICT", ParFlatSPRegions, -1, "Conflict" },
    { "VARIANT", ParFlatSPRegions, -1, "Variant" },
    { "SE_CYS", ParFlatSPRegions, -1, "Selenocysteine" },
    { "VARSPLIC", ParFlatSPRegions, -1, "Splicing variant" },
    { "VAR_SEQ", ParFlatSPRegions, -1, "Splicing variant" },
    { "COILED", ParFlatSPRegions, -1, "Coiled-coil region" },
    { "COMPBIAS", ParFlatSPRegions, -1, "Compositionally biased region" },
    { "MOTIF", ParFlatSPRegions, -1, "Short sequence motif of biological interest" },
    { "REGION", ParFlatSPRegions, -1, "Region of interest in the sequence" },
    { "TOPO_DOM", ParFlatSPRegions, -1, "Topological domain" },
    { "INTRAMEM", ParFlatSPRegions, -1, "Intramembrane region" },
    { "UNSURE", ParFlatSPImports, -1, "unsure" },
    { "INIT_MET", ParFlatSPInitMet, -1, "INIT_MET" },
    { "NON_TER", ParFlatSPNonTer, -1, "NON_TER" },
    { "NON_CONS", ParFlatSPNonCons, -1, "NON_CONS" },
    { NULL, 0, 0, NULL }
};

/* for array index, MOD_RES in the "ParFlat_SPFeat"
 */
#define ParFlatSPSitesModB   9          /* beginning */
#define ParFlatSPSitesModE 174          /* end */

#define COPYRIGHT          "This Swiss-Prot entry is copyright."
#define COPYRIGHT1         "Copyrighted by the UniProt Consortium,"

#define SPDE_RECNAME    000001
#define SPDE_ALTNAME    000002
#define SPDE_SUBNAME    000004
#define SPDE_FLAGS      000010
#define SPDE_INCLUDES   000020
#define SPDE_CONTAINS   000040
#define SPDE_FULL       000100
#define SPDE_SHORT      000200
#define SPDE_EC         000400
#define SPDE_ALLERGEN   001000
#define SPDE_BIOTECH    002000
#define SPDE_CD_ANTIGEN 004000
#define SPDE_INN        010000

typedef struct _char_int_len {
    const char *str;
    Int4       num;
    Int4       len;
} CharIntLen, *CharIntLenPtr;

typedef struct _sprot_de_fields {
    Int4                         tag;
    char*                      start;
    char*                      end;
    struct _sprot_de_fields* next;
} SPDEFields, *SPDEFieldsPtr;

typedef struct sprot_feat_input {
    std::string                  key;           /* column 6-13 */
    std::string                  from;          /* column 15-20 */
    std::string                  to;            /* column 22-27 */
    std::string                  descrip;       /* column 35-75, continue
                                                   line if a blank key */
    struct sprot_feat_input* next;          /* next FT */

    sprot_feat_input() :
        next(NULL) {}

} SPFeatInput, *SPFeatInputPtr;

typedef struct sprot_feat_boolean {
    bool    initmet;
    bool    nonter;
    bool    noright;
    bool    noleft;
} SPFeatBln, *SPFeatBlnPtr;

/* segment location, data from NON_CONS
 */
typedef struct sprot_seg_location {
    Int4                           from;        /* the beginning point of the
                                                   segment */
    Int4                           len;         /* total length of the
                                                   segment */
    struct sprot_seg_location* next;
} SPSegLoc, *SPSegLocPtr;

typedef struct set_of_syns {
    char*                 synname;
    struct set_of_syns* next;
} SetOfSyns, *SetOfSynsPtr;

typedef struct set_of_species {
    char*      fullname;
    char*      name;
    SetOfSynsPtr syn;
} SetOfSpecies, *SetOfSpeciesPtr;

typedef struct _viral_host {
    Int4                    taxid;
    char*                 name;
    struct _viral_host* next;
} ViralHost, *ViralHostPtr;

CharIntLen spde_tags[] = {
    {"RecName:",    SPDE_RECNAME,     8},
    {"AltName:",    SPDE_ALTNAME,     8},
    {"SubName:",    SPDE_SUBNAME,     8},
    {"Includes:",   SPDE_INCLUDES,    9},
    {"Contains:",   SPDE_CONTAINS,    9},
    {"Flags:",      SPDE_FLAGS,       6},
    {"Full=",       SPDE_FULL,        5},
    {"Short=",      SPDE_SHORT,       6},
    {"EC=",         SPDE_EC,          3},
    {"Allergen=",   SPDE_ALLERGEN,    9},
    {"Biotech=",    SPDE_BIOTECH,     8},
    {"CD_antigen=", SPDE_CD_ANTIGEN, 11},
    {"INN=",        SPDE_INN,         4},
    {NULL,          0,                0},
};

const char *org_mods[] = {
    "STRAIN",    "SUBSTRAIN", "TYPE",     "SUBTYPE",  "VAR.",     "SEROTYPE",
    "SEROGROUP", "SEROVAR",   "CULTIVAR", "PATHOVAR", "CHEMOVAR", "BIOVAR",
    "BIOTYPE",   "GROUP",     "SUBGROUP", "ISOLATE",  "ACRONYM",  "DOSAGE",
    "NAT_HOST",  "SUBSP.",    NULL
};

const char *obsolete_dbs[] = {
    "2DBASE-ECOLI",           "AARHUS/GHENT-2DPAGE",    "AGD",
    "ANU-2DPAGE",             "BURULIST",               "CARBBANK",
    "CMR",                    "CORNEA-2DPAGE",          "DICTYDB",
    "DOMO",                   "ECO2DBASE",              "GCRDB",
    "GENEVESTIGATOR",         "GENEW",                  "GENOMEREVIEWS",
    "GERMONLINE",             "HIV",                    "HSC-2DPAGE",
    "HSSP",                   "IPI",                    "LINKHUB",
    "LISTILIST",              "MAIZE-2DPAGE",           "MENDEL",
    "MGD",                    "MYPULIST",               "NMPDR",
    "PATHWAY_INTERACTION_DB", "PHCI-2DPAGE",            "PHOSSITE",
    "PPTASEDB",               "PROTCLUSTDB",            "PHOTOLIST",
    "PMMA-2DPAGE",            "RAT-HEART-2DPAGE",       "RZPD-PROTEXP",
    "SAGALIST",               "SIENA-2DPAGE",           "STYGENE",
    "SUBTILIST",              "TIGR",                   "TRANSFAC",
    "WORMPEP",                "YEPD",                   "YPD",
    NULL
};

const char *valid_dbs[] = {
    "ALLERGOME",              "ARACHNOSERVER",          "ARAPORT",
    "ARRAYEXPRESS",           "BEEBASE",                "BGD",
    "BGEE",                   "BINDINGDB",              "BIOCYC",
    "BIOGRID",                "BIOMUTA",                "BRENDA",
    "CAZY",                   "CCDS",                   "CDD",
    "CGD",                    "CHEMBL",                 "CHITARS",
    "CLEANEX",                "COLLECTF",               "COMPLUYEAST-2DPAGE",
    "CONOSERVER",             "CTD",                    "CYGD",
    "DBSNP",                  "DEPOD",                  "DICTYBASE",
    "DIP",                    "DISGENET",               "DISPROT",
    "DMDM",                   "DNASU",                  "DOSAC-COBS-2DPAGE",
    "DRUGBANK",               "ECHOBASE",               "ECOGENE",
    "EGGNOG",                 "EMBL",                   "ENSEMBL",
    "ENSEMBLBACTERIA",        "ENSEMBLFUNGI",           "ENSEMBLMETAZOA",
    "ENSEMBLPLANTS",          "ENSEMBLPROTISTS",        "EPD",
    "ESTHER",                 "EUHCVDB",                "EUPATHDB",
    "EUROPEPMC",              "EVOLUTIONARYTRACE",      "EXPRESSIONATLAS",
    "FLYBASE",                "GENE3D",                 "GENECARDS",
    "GENEDB",                 "GENEDB_SPOMBE",          "GENEFARM",
    "GENEID",                 "GENEREVIEWS",            "GENETREE",
    "GENEVISIBLE",            "GENEWIKI",               "GENOLIST",
    "GENOMERNAI",             "GK",                     "GLYCOSUITEDB",
    "GRAINGENES",             "GO",                     "GRAMENE",
    "GUIDETOPHARMACOLOGY",    "H-INVDB",                "HAMAP",
    "HGNC",                   "HOGENOM",                "HOVERGEN",
    "HPA",                    "IMGT/GENE-DB",           "IMGT/HLA",
    "IMGT/LIGM",              "IMGT_GENE-DB",           "INPARANOID",
    "INTACT",                 "INTERPRO",               "IPD-KIR",
    "IPTMNET",                "KEGG",                   "KO",
    "LEGIOLIST",              "LEPROMA",                "MAIZEDB",
    "MAIZEGDB",               "MALACARDS",              "MAXQB",
    "MEROPS",                 "MGI",                    "MIM",
    "MINT",                   "MIRBASE",                "MOONPROT",
    "MYCOCLAP",               "NEXTBIO",                "NEXTPROT",
    "OGP",                    "OMA",                    "OPENTARGETS",
    "ORPHANET",               "ORTHODB",                "PANTHER",
    "PATRIC",                 "PAXDB",                  "PDB",
    "PDBSUM",                 "PEPTIDEATLAS",           "PEROXIBASE",
    "PFAM",                   "PHARMGKB",               "PHOSPHOSITE",
    "PHOSPHOSITEPLUS",        "PHYLOMEDB",              "PIR",
    "PIRSF",                  "PMAP-CUTDB",             "POMBASE",
    "PR",                     "PR2",                    "PRIDE",
    "PRINTS",                 "PRO",                    "PRODOM",
    "PROMEX",                 "PROSITE",                "PROTEINMODELPORTAL",
    "PROTEOMES",              "PSEUDOCAP",              "REACTOME",
    "REBASE",                 "REFSEQ",                 "REPRODUCTION-2DPAGE",
    "RGD",                    "RZPD",                   "SABIO-RK",
    "SFLD",                   "SGD",                    "SIGNALINK",
    "SIGNALLINK",             "SIGNOR",                 "SMART",
    "SMR",                    "STRING",                 "SUPFAM",
    "SWISS-2DPAGE",           "SWISSLIPIDS",            "SWISSPALM",
    "TAIR",                   "TCDB",                   "TIGRFAMS",
    "TOPDOWNPROTEOMICS",      "TREEFAM",                "TUBERCULIST",
    "UCD-2DPAGE",             "UCSC",                   "UNICARBKB",
    "UNIGENE",                "UNILIB",                 "UNIPATHWAY",
    "UNITE",                  "VBASE2",                 "VECTORBASE",
    "VEGA-TR",                "VEGA-GN",                "VGNC",
    "WBPARASITE",             "WORLD-2DPAGE",           "WORMBASE",
    "XENBASE",                "ZFIN",                   NULL
};

const char *SP_organelle[] = {
    "CHLOROPLAST", "CYANELLE", "MITOCHONDRION", "PLASMID", "NUCLEOMORPH",
    "HYDROGENOSOME", "APICOPLAST", "CHROMATOPHORE",
    "ORGANELLAR CHROMATOPHORE", NULL
};

const char *PE_values[] = {
    "Evidence at protein level",
    "Evidence at transcript level",
    "Inferred from homology",
    "Predicted",
    "Uncertain",
    NULL
};

/**********************************************************
 *
 *   static char* StringCombine(str1, str2, delim):
 *
 *      Return a string which is combined str1 and str2,
 *   put blank between two strings if "blank" = TRUE;
 *   also memory free out str1 and str2.
 *
 **********************************************************/
static void StringCombine(std::string& dest, const std::string& to_add, const Char* delim)
{
    if (to_add.empty())
        return;

    if (delim != NULL && *delim != '\0' && !dest.empty())
        dest += delim[0];

    dest += to_add;
}

/**********************************************************
 *
 *   static CRef<objects::CDbtag> MakeStrDbtag(dbname, str):
 *
 *                                              10-1-93
 *
 **********************************************************/
static CRef<objects::CDbtag> MakeStrDbtag(char* dbname, char* str)
{
    CRef<objects::CDbtag> tag;

    if (dbname != NULL && str != NULL)
    {
        tag.Reset(new objects::CDbtag);
        tag->SetDb(dbname);
        tag->SetTag().SetStr(str);
    }

    return tag;
}

/**********************************************************
    *
    *   static CRef<objects::CDate> MakeDatePtr(str, source):
    *
    *      Return a DatePtr with "std" type if dd-mmm-yyyy
    *   or dd-mmm-yy format; with "str" type if not
    *   a dd-mmm-yyyy format.
    *
    **********************************************************/
static CRef<objects::CDate> MakeDatePtr(char* str, Parser::ESource source)
{
    static Char msg[11];

    CRef<objects::CDate> res(new objects::CDate);

    if (str == NULL)
        return res;

    if (StringChr(str, '-') != NULL && (IS_DIGIT(*str) != 0 || *str == ' '))
    {
        CRef<objects::CDate_std> std_date = get_full_date(str, true, source);
        res->SetStd(*std_date);
        if (XDateCheck(*std_date) != 0)
        {
            StringNCpy(msg, str, 10);
            msg[10] = '\0';
            ErrPostEx(SEV_WARNING, ERR_REFERENCE_IllegalDate,
                        "Illegal date: %s", msg);
        }
    }

    if (res->Which() == objects::CDate::e_not_set)
    {
        res->SetStr(str);
    }

    return res;
}

/**********************************************************/
static void fta_create_pdb_seq_id(objects::CSP_block_Base::TSeqref& refs, char* mol, Uint1 chain)
{
    if (mol == NULL)
        return;

    CRef<objects::CPDB_seq_id> pdb_seq_id(new objects::CPDB_seq_id);
    pdb_seq_id->SetMol(objects::CPDB_mol_id(mol));

    if (chain > 0)
    {
        pdb_seq_id->SetChain(chain);
    }

    CRef<objects::CSeq_id> sid(new objects::CSeq_id);
    sid->SetPdb(*pdb_seq_id);
    refs.push_back(sid);
}

/**********************************************************/
static void MakeChainPDBSeqId(objects::CSP_block_Base::TSeqref& refs, char* mol, char* chain)
{
    char*  fourth;
    char*  p;
    char*  q;
    char*  r;

    bool  bad;
    bool  got;

    if (mol == NULL || chain == NULL)
        return;

    fourth = StringSave(chain);
    for(bad = false, got = false, q = chain; *q != '\0'; q = p)
    {
        while(*q == ' ' || *q == ',')
            q++;
        for(p = q; *p != '\0' && *p != ' ' && *p != ',';)
            p++;
        if(*p != '\0')
            *p++ = '\0';
        r = StringRChr(q, '=');
        if(r == NULL && !got)
        {
            fta_create_pdb_seq_id(refs, mol, 0);
            continue;
        }
        *r = '\0';
        for(r = q; *r != '\0'; r++)
        {
            if(*r == '/')
                continue;
            if(r[1] != '/' && r[1] != '\0')
            {
                while(*r != '/' && *r != '\0')
                    r++;
                r--;
                bad = true;
                continue;
            }
            got = true;
            fta_create_pdb_seq_id(refs, mol, *r);
        }
    }

    if(bad)
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_InvalidPDBCrossRef,
                  "PDB cross-reference \"%s\" contains one or more chain identifiers that are more than a single character in length.",
                  fourth);
        if(!got)
            fta_create_pdb_seq_id(refs, mol, 0);
    }

    MemFree(fourth);
}

/**********************************************************
 *
 *   static void MakePDBSeqId(refs, mol, rel, chain, drop, source):
 *
 *                                              10-1-93
 *
 **********************************************************/
static void MakePDBSeqId(objects::CSP_block_Base::TSeqref& refs, char* mol, char* rel, char* chain,
                         unsigned char* drop, Parser::ESource source)
{
    if (mol == NULL)
        return;

    if(chain == NULL)
    {
        CRef<objects::CPDB_seq_id> pdb_seq_id(new objects::CPDB_seq_id);
        pdb_seq_id->SetMol(objects::CPDB_mol_id(mol));

        if (rel != NULL)
        {
            CRef<objects::CDate> date = MakeDatePtr(rel, source);
            pdb_seq_id->SetRel(*date);
        }

        CRef<objects::CSeq_id> sid(new objects::CSeq_id);
        sid->SetPdb(*pdb_seq_id);
        refs.push_back(sid);
    }
    else
        MakeChainPDBSeqId(refs, mol, chain);
}

/**********************************************************/
static void GetIntFuzzPtr(Uint1 choice, Int4 a, Int4 b, objects::CInt_fuzz& fuzz)
{
    if (choice < 1 || choice > 4)
        return;

    if (choice == 2)
    {
        fuzz.SetRange().SetMax(a);
        if (b >= 0)
            fuzz.SetRange().SetMin(b);
    }
    else if (choice == 4)
    {
        fuzz.SetLim(static_cast<objects::CInt_fuzz::ELim>(a));
    }
}

/**********************************************************/
static Uint1 GetSPGenome(DataBlkPtr dbp)
{
    DataBlkPtr subdbp;
    char*    p;
    Int4       gmod;

    for(gmod = -1; dbp != NULL; dbp = dbp->next)
        if(dbp->type == ParFlatSP_OS)
        {
            subdbp = (DataBlkPtr) dbp->data;
            for(; subdbp != NULL; subdbp = subdbp->next)
                if(subdbp->type == ParFlatSP_OG)
                {
                    p = subdbp->offset + ParFlat_COL_DATA_SP;
                    if(StringNICmp(p, "Plastid;", 8) == 0)
                        for(p += 8; *p == ' ';)
                            p++;
                    gmod = StringMatchIcase(SP_organelle, p);
                }
        }
    if(gmod == -1)
        return(0);
    if(gmod == 0)
        return(2);                      /* chloroplast */
    if(gmod == 1)
        return(12);                     /* cyanelle */
    if(gmod == 2)
        return(5);                      /* mitochondrion */
    if(gmod == 3)
        return(9);                      /* plasmid */
    if(gmod == 4)
        return(15);                     /* nucleomorph */
    if(gmod == 5)
        return(20);                     /* hydrogenosome */
    if(gmod == 6)
        return(16);                     /* apicoplast */
    if(gmod == 7 || gmod == 8)
        return(22);                     /* chromatophore */
    return(0);
}

/**********************************************************/
static void SpAddToIndexBlk(DataBlkPtr entry, IndexblkPtr pIndex)
{
    char* eptr;
    char* offset;
    size_t len = 0;

    offset = SrchNodeType(entry, ParFlatSP_ID, &len);
    if(offset == NULL || len == 0)
        return;

    eptr = offset + len - 1;
    if(len > 5 && StringNCmp(eptr - 3, "AA.", 3) == 0)
        eptr -= 4;

    while(*eptr == ' ' && eptr > offset)
        eptr--;
    while(IS_DIGIT(*eptr) != 0 && eptr > offset)
        eptr--;
    pIndex->bases = atoi(eptr + 1);
    while(*eptr == ' ' && eptr > offset)
        eptr--;
    if(*eptr == ';')
        eptr--;
    while(IS_ALPHA(*eptr) != 0 && eptr > offset)
        eptr--;

    StringNCpy(pIndex->division, eptr + 1, 3);
    pIndex->division[3] = '\0';
}

/**********************************************************
 *
 *   static void GetSprotSubBlock(pp, entry):
 *
 *                                              9-23-93
 *
 **********************************************************/
static void GetSprotSubBlock(ParserPtr pp, DataBlkPtr entry)
{
    DataBlkPtr dbp;

    dbp = TrackNodeType(entry, ParFlatSP_OS);
    if(dbp != NULL)
    {
        BuildSubBlock(dbp, ParFlatSP_OG, "OG");
        BuildSubBlock(dbp, ParFlatSP_OC, "OC");
        BuildSubBlock(dbp, ParFlatSP_OX, "OX");
        BuildSubBlock(dbp, ParFlatSP_OH, "OH");
        GetLenSubNode(dbp);
    }

    dbp = TrackNodeType(entry, ParFlatSP_RN);
    for(; dbp != NULL; dbp = dbp->next)
    {
        if(dbp->type != ParFlatSP_RN)
            continue;

        BuildSubBlock(dbp, ParFlatSP_RP, "RP");
        BuildSubBlock(dbp, ParFlatSP_RC, "RC");
        BuildSubBlock(dbp, ParFlatSP_RM, "RM");
        BuildSubBlock(dbp, ParFlatSP_RX, "RX");
        BuildSubBlock(dbp, ParFlatSP_RG, "RG");
        BuildSubBlock(dbp, ParFlatSP_RA, "RA");
        BuildSubBlock(dbp, ParFlatSP_RT, "RT");
        BuildSubBlock(dbp, ParFlatSP_RL, "RL");
        GetLenSubNode(dbp);
        dbp->type = ParFlat_REF_END;    /* swiss-prot only has one type */
    }
}

/**********************************************************
 *
 *   static char* GetSPDescrTitle(bptr, eptr, fragment)
 *
 *      Return title string without "(EC ...)" and
 *   "(FRAGMENT)".
 *
 *                                              10-8-93
 *
 **********************************************************/
static char* GetSPDescrTitle(char* bptr, char* eptr, bool* fragment)
{
    const char *tag;
    char*    ptr;
    char*    str;
    char*    p;
    char*    q;
    Char       symb;
    Int4       shift;
    bool       ret;

    str = GetBlkDataReplaceNewLine(bptr, eptr, ParFlat_COL_DATA_SP);
    StripECO(str);

    ptr = StringStr(str, "(GENE NAME");
    if(ptr != NULL)
    {
        ErrPostStr(SEV_WARNING, ERR_GENENAME_DELineGeneName,
                   "Old format, found gene_name in the DE data line");
    }

    ShrinkSpaces(str);

    /* Delete (EC ...)
     */
    if(StringNICmp(str, "RecName: ", 9) == 0 ||
       StringNICmp(str, "AltName: ", 9) == 0 ||
       StringNICmp(str, "SubName: ", 9) == 0)
    {
        tag = "; EC=";
        symb = ';';
        shift = 5;
    }
    else
    {
        tag = "(EC ";
        symb = ')';
        shift = 4;
    }

    for(ptr = str;;)
    {
        ptr = StringStr(ptr, tag);
        if(ptr == NULL)
            break;

        for(p = ptr + shift; *p == ' ';)
            p++;

        if(*p == symb || *p == '\0')
        {
            ptr = p;
            continue;
        }

        while(*p == '.' || *p == '-' || *p == 'n' || IS_DIGIT(*p) != 0)
            p++;
        if(symb == ')')
            while(*p == ' ' || *p == ')')
                p++;

        fta_StringCpy(ptr, p);
    }

    if(symb == ';')
    {
        for(ptr = str;;)
        {
            ptr = StringIStr(ptr, "; Flags:");
            if(ptr == NULL)
                break;
            if(ptr[8] == '\0')
            {
                *ptr = '\0';
                break;
            }
            if(ptr[8] != ' ')
            {
                ptr += 8;
                continue;;
            }
            for(q = ptr + 8;;)
            {
                p = StringChr(q, ':');
                q = StringIStr(q, " Fragment");
                if(q == NULL || (p != NULL && q > p))
                    break;

                ret = true;
                if(q[9] == ';')
                    fta_StringCpy(q, q + 10);
                else if(q[9] == '\0')
                    *q = '\0';
                else if(q[9] == 's' || q[9] == 'S')
                {
                    if(q[10] == ';')
                        fta_StringCpy(q, q + 11);
                    else if(q[10] == '\0')
                        *q = '\0';
                    else
                    {
                        q++;
                        ret = false;
                    }
                }
                else
                {
                    q++;
                    ret = false;
                }
                if(ret)
                    *fragment = true;
            }
            if(ptr[8] == '\0')
            {
                *ptr = '\0';
                break;
            }
            q = StringChr(ptr + 8, ';');
            p = StringChr(ptr + 8, ':');
            if(q == NULL)
            {
                if(p == NULL)
                    break;
                else
                    fta_StringCpy(ptr + 2, ptr + 9);
            }
            else
            {
                if(p == NULL)
                    ptr += 9;
                else
                {
                    if(p < q)
                        fta_StringCpy(ptr + 2, ptr + 9);
                    else
                        ptr += 9;
                }
            }
        }
    }
    else
    {
        ptr = StringIStr(str, "(FRAGMENT");
        if(ptr != NULL)
        {   
            /* delete (FRAGMENTS) or (FRAGMENT)
             */
            *fragment = true;

            for(p = ptr + 8; *p != '\0' && *p != ')';)
                p++;
            while(*p == ' ' || *p == ')')
                p++;

            fta_StringCpy(ptr, p);
        }
    }

    ptr = tata_save(str);
    p = ptr + StringLen(ptr) - 1;
    if(*p == '.')
    {
        while(p > ptr && *(p - 1) == ' ')
            p--;
        *p = '.';
        p[1] = '\0';
    }
    MemFree(str);
    return(ptr);
}

/**********************************************************/
static char* GetLineOSorOC(DataBlkPtr dbp, const char *pattern)
{
    char* res;
    char* p;
    char* q;

    size_t len = dbp->len;
    if(len == 0)
        return(NULL);
    for(size_t i = 0; i < dbp->len; i++)
        if(dbp->offset[i] == '\n')
            len -= 5;
    res = (char*) MemNew(len);
    p = res;
    for(q = dbp->offset; *q != '\0';)
    {
        if(StringNCmp(q, pattern, 5) != 0)
            break;
        if(p > res)
            *p++ = ' ';
        for(q += 5; *q != '\n' && *q != '\0'; q++)
            *p++ = *q;
        if(*q == '\n')
            q++;
    }
    *p = '\0';
    if(p > res)
        p--;
    while(*p == '.' || *p == ' ' || *p == '\t')
    {
        *p = '\0';
        if(p > res)
            p--;
    }
    return(res);
}

/**********************************************************/
static SetOfSpeciesPtr GetSetOfSpecies(char* line)
{
    SetOfSpeciesPtr res;
    SetOfSynsPtr    ssp;
    SetOfSynsPtr    tssp;
    char*         p;
    char*         q;
    char*         r;
    char*         temp;
    Int2            i;

    if(line == NULL || line[0] == '\0')
        return(NULL);
    for(p = line; *p == ' ' || *p == '\t' || *p == '.' || *p == ',';)
        p++;
    if(*p == '\0')
        return(NULL);

    res = (SetOfSpeciesPtr) MemNew(sizeof(SetOfSpecies));
    res->fullname = StringSave(p);
    res->name = NULL;
    res->syn = NULL;

    temp = StringSave(res->fullname);
    p = StringChr(temp, '(');
    if(p == NULL)
        res->name = StringSave(temp);
    else
    {
        *p = '\0';
        q = temp;
        if(p > q)
        {
            for(r = p - 1; *r == ' ' || *r == '\t'; r--)
            {
                *r = '\0';
                if(r == q)
                    break;
            }
        }
        res->name = StringSave(temp);
        *p = '(';
        ssp = (SetOfSynsPtr) MemNew(sizeof(SetOfSyns));
        ssp->synname = NULL;
        ssp->next = NULL;
        tssp = ssp;
        for(;;)
        {
            for(p++; *p == ' ' || *p == '\t';)
                p++;
            q = p;
            for(i = 1; *p != '\0'; p++)
            {
                if(*p == '(')
                    i++;
                else if(*p == ')')
                    i--;
                if(i == 0)
                    break;
            }
            if(*p == '\0')
            {
                tssp->next = (SetOfSynsPtr) MemNew(sizeof(SetOfSyns));
                tssp = tssp->next;
                tssp->synname = StringSave(q);
                tssp->next = NULL;
                break;
            }
            *p = '\0';
            if(p > q)
            {
                for(r = p - 1; *r == ' ' || *r == '\t'; r--)
                {
                    *r = '\0';
                    if(r == q)
                        break;
                }
            }
            tssp->next = (SetOfSynsPtr) MemNew(sizeof(SetOfSyns));
            tssp = tssp->next;
            tssp->synname = StringSave(q);
            tssp->next = NULL;
            *p = ')';
            p = StringChr(p, '(');
            if(p == NULL)
                break;
        }

        res->syn = ssp->next;
        MemFree(ssp);
    }

    MemFree(temp);
    return(res);
}

/**********************************************************/
static void fix_taxname_dot(objects::COrg_ref& org_ref)
{
    if (!org_ref.IsSetTaxname())
        return;

    std::string& taxname = org_ref.SetTaxname();

    size_t len = taxname.size();
    if (len < 3)
        return;

    const Char* p = taxname.c_str() + len - 3;
    if((p[0] == ' ' || p[0] == '\t') && (p[1] == 's' || p[1] == 'S') &&
       (p[2] == 'p' || p[2] == 'P') && p[3] == '\0')
    {
        if(StringICmp(taxname.c_str(), "BACTERIOPHAGE SP") == 0)
            return;

        taxname += ".";
    }
}

/**********************************************************/
static CRef<objects::COrg_ref> fill_orgref(SetOfSpeciesPtr sosp)
{
    SetOfSynsPtr synsp;

    const char   **b;

    char*      p;
    char*      q;
    Uint1        num;
    size_t       i;

    CRef<objects::COrg_ref> org_ref;

    if (sosp == NULL)
        return org_ref;

    org_ref.Reset(new objects::COrg_ref);

    if(sosp->name != NULL && sosp->name[0] != '\0')
        org_ref->SetTaxname(sosp->name);

    for(synsp = sosp->syn; synsp != NULL; synsp = synsp->next)
    {
        p = synsp->synname;
        if(p == NULL || *p == '\0')
            continue;

        q = StringIStr(p, "PLASMID");
        if(q == NULL)
            q = StringIStr(p, "CLONE");
        if(q != NULL)
        {
            i = (*q == 'C' || *q == 'c') ? 5 : 7;
            if(q > p)
            {
                q--;
                i++;
            }
            if((q == p || q[0] == ' ' || q[0] == '\t') &&
               (q[i] == ' ' || q[i] == '\t' || q[i] == '\0'))
            {
                if (!org_ref->IsSetTaxname())
                    org_ref->SetTaxname(p);
                else
                {
                    std::string& taxname = org_ref->SetTaxname();
                    taxname += " (";
                    taxname += p;
                    taxname += ")";
                }
                continue;
            }
        }

        if((StringNICmp("PV.", p, 3) == 0 && (p[3] == ' ' || p[3] == '\t' || p[3] == '\0')) ||
           StringICmp(p, "AD11A") == 0 || StringICmp(p, "AD11P") == 0)
        {
            if (!org_ref->IsSetTaxname())
                org_ref->SetTaxname(p);
            else
            {
                std::string& taxname = org_ref->SetTaxname();
                taxname += " (";
                taxname += p;
                taxname += ")";
            }
            continue;
        }

        for(q = p; *p != '\0' && *p != ' ' && *p != '\t';)
            p++;
        if(*p == '\0')
        {
            org_ref->SetSyn().push_back(q);
            continue;
        }

        *p = '\0';
        for(q = p + 1; *q == ' ' || *q == '\t';)
            q++;

        if(StringICmp(synsp->synname, "COMMON") == 0)
        {
            if (!org_ref->IsSetCommon())
                org_ref->SetCommon(q);
            else
                org_ref->SetSyn().push_back(q);
            continue;
        }

        for(b = org_mods, num = 2; *b != NULL; b++, num++)
            if(StringICmp(synsp->synname, *b) == 0)
                break;
        *p = ' ';

        if(*b == NULL)
        {
            for(b = org_mods, num = 2; *b != NULL; b++, num++)
            {
                if(StringICmp(*b, "ISOLATE") != 0 &&
                   StringICmp(*b, "STRAIN") != 0)
                    continue;
                p = StringIStr(synsp->synname, *b);
                if(p == NULL)
                    continue;

                p--;
                i = StringLen(*b) + 1;
                if (*p == ' ' && (p[i] == ' ' || p[i] == '\t' || p[i] == '\0'))
                {
                    std::string& taxname = org_ref->SetTaxname();
                    taxname += " (";
                    taxname += synsp->synname;
                    taxname += ")";
                    break;
                }
            }

            if (*b == NULL)
                org_ref->SetSyn().push_back(synsp->synname);
            continue;
        }

        std::string& taxname = org_ref->SetTaxname();
        if (!taxname.empty())
            taxname += " ";

        taxname += "(";
        taxname += synsp->synname;
        taxname += ")";
    }

    fix_taxname_dot(*org_ref);
    if (org_ref->IsSetSyn() && org_ref->GetSyn().empty())
        org_ref->ResetSyn();

    return org_ref;
}

/**********************************************************/
static void SetOfSpeciesFree(SetOfSpeciesPtr sosp)
{
    SetOfSynsPtr ssp;
    SetOfSynsPtr tssp;

    if(sosp->fullname != NULL)
        MemFree(sosp->fullname);
    if(sosp->name != NULL)
        MemFree(sosp->name);
    for(ssp = sosp->syn; ssp != NULL; ssp = tssp)
    {
        tssp = ssp->next;
        if(ssp->synname != NULL)
            MemFree(ssp->synname);
        MemFree(ssp);
    }
    MemFree(sosp);
}

/**********************************************************/
static ViralHostPtr GetViralHostsFrom_OH(DataBlkPtr dbp)
{
    ViralHostPtr vhp;
    ViralHostPtr tvhp;
    char*      line;
    char*      p;
    char*      q;
    char*      r;
    Char         ch;

    for(; dbp != NULL; dbp = dbp->next)
        if(dbp->type == ParFlatSP_OS)
            break;
    if(dbp == NULL)
        return(NULL);

    for(dbp = (DataBlkPtr) dbp->data; dbp != NULL; dbp = dbp->next)
        if(dbp->type == ParFlatSP_OH)
            break;
    if(dbp == NULL)
        return(NULL);

    vhp = (ViralHostPtr) MemNew(sizeof(ViralHost));
    vhp->name = NULL;
    vhp->taxid = 0;
    vhp->next = NULL;
    tvhp = vhp;

    line = (char*) MemNew(dbp->len + 2);
    ch = dbp->offset[dbp->len-1];
    dbp->offset[dbp->len-1] = '\0';
    line[0] = '\n';
    line[1] = '\0';
    StringCat(line, dbp->offset);
    dbp->offset[dbp->len-1] = ch;

    if(StringNICmp(line, "\nOH   NCBI_TaxID=", 17) != 0)
    {
        ch = '\0';
        p = StringChr(line + 1, '\n');
        if(p != NULL)
            *p = '\0';
        if(StringLen(line + 1) > 20)
        {
            ch = line[21];
            line[21] = '\0';
        }
        ErrPostEx(SEV_ERROR, ERR_SOURCE_UnknownOHType,
                  "Unknown beginning of OH block: \"%s[...]\".", line + 1);
        if(ch != '\0')
            line[21] = ch;
        if(p != NULL)
            *p = '\n';
    }

    for(p = line;;)
    {
        p = StringIStr(p, "\nOH   NCBI_TaxID=");
        if(p == NULL)
            break;
        for(p += 17, q = p; *q == ' ';)
            q++;
        r = StringChr(q, '\n');
        p = StringChr(q, ';');
        if((r == NULL || r > p) && p != NULL)
        {
            tvhp->next = (ViralHostPtr) MemNew(sizeof(ViralHost));
            tvhp = tvhp->next;
            tvhp->next = NULL;
            for(p--; *p == ';' || *p == ' ';)
                p--;
            p++;
            for(r = q; *r >= '0' && *r <= '9';)
                r++;
            ch = *p;
            *p = '\0';
            if(r != p)
            {
                ErrPostEx(SEV_ERROR, ERR_SOURCE_InvalidNcbiTaxID,
                          "Invalid NCBI TaxID in OH line : \"%s\".", q);
                tvhp->taxid = 0;
            }
            else
                tvhp->taxid = atoi(q);
            for(p++; *p == ' ' || *p == ';';)
                p++;
            r = StringChr(p, '\n');
            if(r == NULL)
                r = p + StringLen(p);
            else
                r--;
            while((*r == ' ' || *r == '.' || *r == '\0') && r > p)
                r--;
            if(*r != '\0' && *r != '.' && *r != ' ')
                r++;
            ch = *r;
            *r = '\0';
            tvhp->name = StringSave(p);
            ShrinkSpaces(tvhp->name);
            *r = ch;
            p = r;
        }
        else
        {
            if(r != NULL)
                *r = '\0';
            ErrPostEx(SEV_ERROR, ERR_SOURCE_IncorrectOHLine,
                      "Incorrect OH line content skipped: \"%s\".", q);
            if(r != NULL)
                *r = '\n';
            p = q;
        }
    }
    MemFree(line);

    tvhp = vhp->next;
    MemFree(vhp);

    if(tvhp == NULL)
        ErrPostEx(SEV_WARNING, ERR_SOURCE_NoNcbiTaxIDLookup,
                  "No legal NCBI TaxIDs found in OH line.");

    return(tvhp);
}

/**********************************************************/
static Int4 GetTaxIdFrom_OX(DataBlkPtr dbp)
{
    DataBlkPtr subdbp;
    char*    line;
    char*    p;
    char*    q;
    bool       got;
    Char       ch;
    Int4       taxid;

    for(got = false, taxid = 0; dbp != NULL; dbp = dbp->next)
    {
        if(dbp->type != ParFlatSP_OS)
            continue;

        subdbp = (DataBlkPtr) dbp->data;
        for(; subdbp != NULL; subdbp = subdbp->next)
        {
            if(subdbp->type != ParFlatSP_OX)
                continue;
            got = true;
            ch = subdbp->offset[subdbp->len-1];
            subdbp->offset[subdbp->len-1] = '\0';
            line = StringSave(subdbp->offset);
            subdbp->offset[subdbp->len-1] = ch;

            p = StringChr(line, '\n');
            if(p != NULL)
                *p = '\0';
            if(StringNICmp(line, "OX   NCBI_TaxID=", 16) != 0)
            {
                if(StringLen(line) > 20)
                    line[20] = '\0';
                ErrPostEx(SEV_ERROR, ERR_SOURCE_UnknownOXType,
                          "Unknown beginning of OX line: \"%s\".", line);
                MemFree(line);
                break;
            }
            p = StringChr(line + 16, ';');
            if(p != NULL)
            {
                *p++ = '\0';
                for(q = p; *q == ' ';)
                    q++;
                if(*q != '\0')
                {
                    ErrPostEx(SEV_ERROR, ERR_FORMAT_UnexpectedData,
                              "Encountered unexpected data while parsing OX line: \"%s\" : Ignored.",
                              p);
                }
            }
            for(p = line + 16; *p == ' ';)
                p++;
            if(*p == '\0')
            {
                MemFree(line);
                break;
            }
            for(q = p; *q >= '0' && *q <= '9';)
                q++;
            if(*q == ' ' || *q == '\0')
                taxid = atoi(p);
            if(taxid < 1 || (*q != ' ' && *q != '\0'))
            {
                ErrPostEx(SEV_ERROR, ERR_SOURCE_InvalidNcbiTaxID,
                          "Invalid NCBI TaxID on OX line : \"%s\" : Ignored.",
                          p);
            }
            MemFree(line);
            break;
        }
        break;
    }

    if(got && taxid < 1)
        ErrPostEx(SEV_WARNING, ERR_SOURCE_NoNcbiTaxIDLookup,
                  "No legal NCBI TaxID found on OX line : will use organism names for lookup instead.");

    return(taxid);
}

/**********************************************************/
static CRef<objects::COrg_ref> GetOrganismFrom_OS_OC(DataBlkPtr entry)
{
    SetOfSpeciesPtr sosp;
    DataBlkPtr      dbp;
    char*         line_OS;
    char*         line_OC;

    CRef<objects::COrg_ref> org_ref;

    line_OS = NULL;
    line_OC = NULL;

    for(dbp = entry; dbp != NULL; dbp = dbp->next)
    {
        if(dbp->type != ParFlatSP_OS)
            continue;
        line_OS = GetLineOSorOC(dbp, "OS   ");
        for(dbp = (DataBlkPtr) dbp->data; dbp != NULL; dbp = dbp->next)
        {
            if(dbp->type != ParFlatSP_OC)
                continue;
            line_OC = GetLineOSorOC(dbp, "OC   ");
            break;
        }
        break;
    }

    if (line_OS != NULL && line_OS[0] != '\0')
    {
        sosp = GetSetOfSpecies(line_OS);
        if (sosp != NULL && sosp->name != NULL && sosp->name[0] != '\0')
        {
            org_ref = fill_orgref(sosp);
        }

        SetOfSpeciesFree(sosp);
        MemFree(line_OS);
    }

    if (org_ref.NotEmpty() && line_OC != NULL && line_OC[0] != '\0')
    {
        org_ref->SetOrgname().SetLineage(line_OC);
    }

    return org_ref;
}

/**********************************************************/
static void get_plasmid(DataBlkPtr entry, objects::CSP_block::TPlasnm& plasms)
{
    DataBlkPtr dbp;
    DataBlkPtr subdbp;
    char*    offset = NULL;
    char*    eptr = NULL;
    char*    str;
    char*    ptr;
    Int4       gmod = -1;
    
    dbp = TrackNodeType(entry, ParFlatSP_OS);
    for(; dbp != NULL; dbp = dbp->next)
    {
        if(dbp->type != ParFlatSP_OS)
            continue;

        subdbp = (DataBlkPtr) dbp->data;
        for(; subdbp != NULL; subdbp = subdbp->next)
        {
            if(subdbp->type != ParFlatSP_OG)
                continue;

            offset = subdbp->offset + ParFlat_COL_DATA_SP;
            eptr = offset + subdbp->len;
            gmod = StringMatchIcase(SP_organelle, offset);
        }
    }
    if(gmod != Seq_descr_GIBB_mod_plasmid)
        return;

    while((str = StringIStr(offset, "PLASMID")) != NULL)
    {
        if(str > eptr)
            break;

        str += StringLen("PLASMID");
        while(*str == ' ')
            str++;

        for(ptr = str; *ptr != '\n' && *ptr != ' ';)
            ptr++;
        ptr--;
        if(ptr > str)
        {
            plasms.push_back(std::string(str, ptr));
        }
        else
            ErrPostEx(SEV_ERROR, ERR_SOURCE_MissingPlasmidName,
                      "Plasmid name is missing from OG line of SwissProt record.");
        offset = ptr;
    }
}

/**********************************************************
 *
 *   static char* GetDRToken(ptr):
 *
 *      From GetTheCurrentToken.
 *
 **********************************************************/
static char* GetDRToken(char** ptr)
{
    char* ret;
    char* p;

    p = *ptr;
    if(p == NULL || *p == '\0')
        return(NULL);

    for(;; p++)
    {
        if(*p == '\0' || *p == '\n')
            break;
        if((*p == ';' || *p == '.') && (p[1] == ' ' || p[1] == '\n'))
            break;
    }

    if(*p == '\0' || *p == '\n')
        return(NULL);

    *p++ = '\0';

    ret = *ptr;

    while(*p == ' ' || *p == ';' || *p == '.')
        p++;
    *ptr = p;

    if(*ret == '\0')
        return(NULL);
    return(ret);
}

/**********************************************************/
static CRef<objects::CSeq_id> AddPIDToSeqId(char* str, char* acc)
{
    long        lID;
    char*     end = NULL;

    CRef<objects::CSeq_id> sid;

    if(str == NULL || *str == '\0')
        return sid;

    if(str[0] == '-')
    {
        ErrPostEx(SEV_WARNING, ERR_SPROT_DRLine,
                  "Not annotated CDS [ACC:%s, PID:%s]", acc, str);
        return sid;
    }
    errno = 0;                  /* clear errors, the error flag from stdlib */
    lID = strtol(str + 1, &end, 10);
    if((lID == 0 && str + 1 == end) || (lID == LONG_MAX && errno == ERANGE))
    {
        /* Bad or too large number
         */
        ErrPostEx(SEV_WARNING, ERR_SPROT_DRLine,
                  "Invalid PID value [ACC:%s, PID:%s]", acc, str);
        return sid;
    }

    if (*str == 'G')
    {
        sid.Reset(new objects::CSeq_id);
        sid->SetGi( GI_FROM(long, lID) );
    }
    else if(*str == 'E' || *str == 'D')
    {
        CRef<objects::CDbtag> tag(new objects::CDbtag);
        tag->SetDb("PID");
        tag->SetTag().SetStr(str);

        sid.Reset(new objects::CSeq_id);
        sid->SetGeneral(*tag);
    }
    else
    {
        ErrPostEx(SEV_WARNING, ERR_SPROT_DRLine,
                  "Unrecognized PID data base type [ACC:%s, PID:%s]",
                  acc, str);
    }
    return sid;
}

/**********************************************************/
static bool AddToList(ValNodePtr* head, char* str)
{
    ValNodePtr vnp;
    char*    data;
    char*    dot;
    char*    d;

    if(str == NULL)
        return false;

    if(str[0] == '-' && str[1] == '\0')
        return true;

    dot = StringChr(str, '.');
    for(vnp = *head; vnp != NULL; vnp = vnp->next)
        if(StringCmp((char*) vnp->data.ptrvalue, str) == 0)
            break;
    if(vnp != NULL)
        return false;

    if(dot != NULL)
    {
        *dot = '\0';
        for(vnp = *head; vnp != NULL; vnp = vnp->next)
        {
            data = (char*) vnp->data.ptrvalue;
            d = StringChr(data, '.');
            if(d == NULL)
                continue;
            *d = '\0';
            if(StringCmp(data, str) == 0)
            {
                ErrPostEx(SEV_WARNING, ERR_SPROT_DRLine,
                          "Same protein accessions with different versions found in DR line [PID1:%s.%s; PID2:%s.%s].",
                          data, d + 1, str, dot + 1);
            }
            *d = '.';
        }
        *dot = '.';
    }
    vnp = ConstructValNode(NULL, 0, StringSave(str));
    ValNodeLink(head, vnp);

    return true;
}

/**********************************************************/
static void CheckSPDupPDBXrefs(objects::CSP_block::TSeqref& refs)
{
    NON_CONST_ITERATE(objects::CSP_block::TSeqref, cur_ref, refs)
    {
        if ((*cur_ref)->Which() != objects::CSeq_id::e_Pdb ||
            (*cur_ref)->GetPdb().IsSetRel())
            continue;

        bool got = false;

        const objects::CPDB_seq_id& cur_id = (*cur_ref)->GetPdb();
        objects::CSP_block::TSeqref::iterator next_ref = cur_ref;

        for (++next_ref; next_ref != refs.end(); )
        {
            if ((*next_ref)->Which() != objects::CSeq_id::e_Pdb ||
                (*next_ref)->GetPdb().IsSetRel())
                continue;

            const objects::CPDB_seq_id& next_id = (*next_ref)->GetPdb();

            if (cur_id.GetMol().Get() != next_id.GetMol().Get())
            {
                ++next_ref;
                continue;
            }

            if (next_id.GetChain() != 32)
            {
                if (!got && cur_id.GetChain() == 32)
                {
                    got = true;
/* Commented out until the proper handling of PDB chain contents
                    ErrPostEx(SEV_WARNING, ERR_FORMAT_DuplicateCrossRef,
                              "Duplicate PDB cross reference removed, mol = \"%s\", chain = \"%d\".",
                              psip1->mol, (int) psip1->chain);
*/
                }
                if (cur_id.GetChain() != next_id.GetChain())
                {
                    ++next_ref;
                    continue;
                }
            }

            next_ref = refs.erase(next_ref);
/* Commented out until the proper handling of PDB chain contents
            ErrPostEx(SEV_WARNING, ERR_FORMAT_DuplicateCrossRef,
                      "Duplicate PDB cross reference removed, mol = \"%s\", chain = \"%d\".",
                      psip2->mol, (int) psip2->chain);
*/
        }
    }
}

/**********************************************************/
static void fta_check_embl_drxref_dups(ValNodePtr embl_acc_list)
{
    ValNodePtr vnp;
    ValNodePtr vnpn;
    char*    n;
    char*    p;
    char*    q;

    if(embl_acc_list == NULL || embl_acc_list->next->next == NULL)
        return;

    for(vnp = embl_acc_list; vnp != NULL; vnp = vnp->next->next)
    {
        p = (char*) vnp->data.ptrvalue;
        q = StringChr(p, '.');
        if(q != NULL)
        {
            for(p = q + 1; *p >= '0' && *p <= '9';)
                p++;
            if(*p != '\0')
                q = NULL;
            p = (char*) vnp->data.ptrvalue;
        }
        n = (char*) vnp->next->data.ptrvalue;
        for(vnpn = vnp->next->next; vnpn != NULL; vnpn = vnpn->next->next)
        {
            if(vnp->next->choice != vnpn->next->choice &&
               StringCmp(p, (char*) vnpn->data.ptrvalue) == 0)
            {
                if(q != NULL)
                    *q = '\0';
                if(GetProtAccOwner(p) > 0)
                    ErrPostEx(SEV_WARNING, ERR_SPROT_DRLineCrossDBProtein,
                              "Protein accession \"%s\" associated with \"%s\" and \"%s\".",
                              (char*) vnpn->data.ptrvalue, n,
                              (char*) vnpn->next->data.ptrvalue);
                if(q != NULL)
                    *q = '.';
            }
        }
        if(q != NULL)
            *q = '.';
    }
}

/**********************************************************
 *
 *   static void GetDRlineDataSP(entry, spbp, drop, source):
 *
 *      Database identifiers on the DR lines which point to
 *   entries in GenBank, EMBL, DDBJ, PIR, or PDB are output
 *   as Seq-id's of the appropriate type:
 *      - For GenBank and DDBJ, only the primary identifier
 *        (accession number) is captured; and their database
 *        references are actually labelled as "EMBL". Their
 *        true nature is determined by the accession number
 *        ownership rules described by accession prefix.
 *      - For EMBL, both the primary and secondary
 *        identifiers are captured.
 *      - For PIR, we only capture the secondary
 *        identifier (name).
 *      - For PDB, we capture both the primary identifier
 *        (molecule name) and the secondary identifier
 *        (release date).
 *           example:
 *                              DR   EMBL; J05536; RNPCBB.
 *                              DR   EMBL; X51318; RN10SP.
 *                              DR   PIR; A36581; A36581.
 *                              DR   PDB; 1CCD; PRELIMINARY.
 *           Release 33.0 Cross-references to EMBL/GenBank/DDBJ
 *
 *                              DR   EMBL; X51318; G63880; -.
 *
 *         seqref {
 *           genbank {
 *             accession "J05536" } ,
 *           embl {
 *             name "RN10SP" ,
 *             accession "X51318" } ,
 *           pir {
 *             name "A36581" } ,
 *           pdb {
 *             mol "1CCD" ,
 *             rel
 *               str "PRELIMINARY" }
 *
 *           Release 33.0
 *
 *         seqref {
 *             gi 63880 ,
 *               } ,
 *
 *      All other databank references are output using Dbtag.
 *   In these cases, secondary identifiers, whether
 *   entry-names, release numbers, or date-stamps, are not
 *   captured since Dbtag has no provision for them.
 *      example:
 *                      DR   PROSITE; PS00403; UTEROGLOBIN_1.
 *                      DR   PROSITE; PS00404; UTEROGLOBIN_2.
 *         dbref {
 *           {
 *             db "PROSITE" ,
 *             tag
 *               str "PS00403" } ,
 *           {
 *             db "PROSITE" ,
 *             tag
 *               str "PS00404" } } ,
 *
 *      Also need to delete duplicated DR line.
 *
 **********************************************************/
static void GetDRlineDataSP(DataBlkPtr entry, objects::CSP_block& spb, unsigned char* drop,
                            Parser::ESource source)
{
    ValNodePtr   embl_vnp;
    ValNodePtr   acc_list = NULL;
    ValNodePtr   pid_list = NULL;
    ValNodePtr   ens_tran_list = NULL;
    ValNodePtr   ens_prot_list = NULL;
    ValNodePtr   ens_gene_list = NULL;
    ValNodePtr   embl_acc_list = NULL;
    const char   **b;
    char*      offset;
    char*      token1;
    char*      token2;
    char*      token3;
    char*      token4;
    char*      token5;
    char*      str;
    char*      ptr;
    char*      p;
    char*      q;
    bool         pdbold;
    bool         pdbnew;
    bool         check_embl_prot;
    size_t       len = 0;
    Uint1        ptype;
    Uint1        ntype;
    Char         ch;

    spb.ResetSeqref();
    spb.ResetDbref();

    offset = SrchNodeType(entry, ParFlatSP_DR, &len);
    if(offset == NULL)
        return;

    ch = offset[len];
    offset[len] = '\0';
    str = (char*) MemNew(len + 2);
    StringCpy(str, "\n");
    StringCat(str, offset);
    offset[len] = ch;
    pdbold = false;
    pdbnew = false;
    embl_acc_list = ValNodeNew(NULL);
    embl_vnp = embl_acc_list;
    check_embl_prot = false;
    for(ptr = str;;)
    {
        if(*drop != 0)
            break;
        ptr = StringChr(ptr, '\n');
        if(ptr == NULL)
            break;
        ptr++;
        if(StringNCmp(ptr, "DR   ", 5) != 0)
            continue;
        ptr += ParFlat_COL_DATA_SP;
        token1 = GetDRToken(&ptr);
        token2 = GetDRToken(&ptr);
        token3 = GetDRToken(&ptr);
        token4 = GetDRToken(&ptr);
        token5 = GetDRToken(&ptr);
        if(token1 == NULL || token2 == NULL || token3 == NULL ||
           (StringCmp(token2, "-") == 0 && StringCmp(token3, "-") == 0))
        {
            ErrPostEx(SEV_ERROR, ERR_SPROT_DRLine,
                      "Badly formatted DR line. Skipped.");
            continue;
        }

        if(StringICmp(token1, "MD5") == 0)
            continue;

        for(b = valid_dbs; *b != NULL; b++)
            if(StringICmp(*b, token1) == 0)
                break;
        if(*b == NULL)
        {
            for(b = obsolete_dbs; *b != NULL; b++)
                if(StringICmp(*b, token1) == 0)
                    break;
            if(*b == NULL)
                ErrPostEx(SEV_WARNING, ERR_DRXREF_UnknownDBname,
                          "Encountered a new/unknown database name in DR line: \"%s\".",
                          token1);
            else
                ErrPostEx(SEV_WARNING, ERR_SPROT_DRLine,
                          "Obsolete database name found in DR line: \"%s\".",
                          token1);
        }

        if(StringICmp(token1, "PDB") == 0)
        {
            if(token4 == NULL)
                pdbold = true;
            else
                pdbnew = true;
            
            MakePDBSeqId(spb.SetSeqref(), token2, token3,
                         (token5 == NULL) ? token4 : token5,
                         drop, source);
        }
        else if(StringICmp(token1, "PIR") == 0)
        {
            CRef<objects::CSeq_id> id(MakeLocusSeqId(token3, objects::CSeq_id::e_Pir));
            if (id.NotEmpty())
                spb.SetSeqref().push_back(id);
        }
        else if(StringICmp(token1, "EMBL") == 0)
        {
            ntype = GetNucAccOwner(token2, false);
            if(ntype == 0)
            {
                ErrPostEx(SEV_ERROR, ERR_SPROT_DRLine,
                          "Incorrect NA accession is used in DR line: \"%s\". Skipped...",
                          token2);
            }
            else if(AddToList(&acc_list, token2))
            {
                CRef<objects::CSeq_id> id(MakeAccSeqId(token2, ntype, false, 0, true, false));
                if (id.NotEmpty())
                    spb.SetSeqref().push_back(id);
            }

            ptype = 0;
            if(token3[0] >= 'A' && token3[0] <= 'Z' &&
               token3[1] >= 'A' && token3[1] <= 'Z')
            {
                p = StringChr(token3, '.');
                if(p != NULL)
                {
                    *p = '\0';
                    ptype = GetProtAccOwner(token3);
                    *p = '.';
                    for(q = p + 1; *q >= '0' && *q <= '9';)
                        q++;
                    if(q == p + 1 || *q != '\0')
                        p = NULL;
                }
                if(p == NULL || ptype == 0)
                {
                    ErrPostEx(SEV_ERROR, ERR_SPROT_DRLine,
                              "Incorrect protein accession is used in DR line [ACC:%s; PID:%s]. Skipped...",
                              token2, token3);
                    continue;
                }
            }
            else
                p = NULL;

            if(ntype > 0)
            {
                embl_vnp->next = ConstructValNode(NULL, ptype,
                                                  StringSave(token3));
                embl_vnp = embl_vnp->next;
                embl_vnp->next = ConstructValNode(NULL, ntype,
                                                  StringSave(token2));
                embl_vnp = embl_vnp->next;
            }

            if(!AddToList(&pid_list, token3))
            {
                check_embl_prot = true;
                continue;
            }

            CRef<objects::CSeq_id> id;
            if (p == NULL)
                id = AddPIDToSeqId(token3, token2);
            else
            {
                *p++ = '\0';
                id = MakeAccSeqId(token3, ptype, true, (Int2)atoi(p),
                                  false, false);
            }

            if (id.NotEmpty())
                spb.SetSeqref().push_back(id);
        }
        else if(StringICmp(token1, "ENSEMBL") == 0 ||
                StringICmp(token1, "ENSEMBLBACTERIA") == 0 ||
                StringICmp(token1, "ENSEMBLFUNGI") == 0 ||
                StringICmp(token1, "ENSEMBLMETAZOA") == 0 ||
                StringICmp(token1, "ENSEMBLPLANTS") == 0 ||
                StringICmp(token1, "ENSEMBLPROTISTS") == 0 ||
                StringICmp(token1, "WORMBASE") == 0)
        {
            if(AddToList(&ens_tran_list, token2))
            {
                CRef<objects::CDbtag> tag = MakeStrDbtag(token1, token2);
                if (tag.NotEmpty())
                    spb.SetDbref().push_back(tag);
            }

            if(!AddToList(&ens_prot_list, token3))
            {
                ErrPostEx(SEV_WARNING, ERR_SPROT_DRLine,
                          "Duplicated protein id \"%s\" in \"%s\" DR line.",
                          token3, token1);
            }
            else
            {
                CRef<objects::CDbtag> tag = MakeStrDbtag(token1, token3);
                if (tag.NotEmpty())
                    spb.SetDbref().push_back(tag);
            }

            if(token4 != NULL && AddToList(&ens_gene_list, token4))
            {
                CRef<objects::CDbtag> tag = MakeStrDbtag(token1, token4);
                if (tag.NotEmpty())
                    spb.SetDbref().push_back(tag);
            }
        }
        else if(StringICmp(token1, "REFSEQ") == 0)
        {
            ptype = 0;
            if(token2[0] >= 'A' && token2[0] <= 'Z' &&
               token2[1] >= 'A' && token2[1] <= 'Z')
            {
                p = StringChr(token2, '.');
                if(p != NULL)
                {
                    *p = '\0';
                    ptype = GetProtAccOwner(token2);
                    *p = '.';
                    for(q = p + 1; *q >= '0' && *q <= '9';)
                        q++;
                    if(q == p + 1 || *q != '\0')
                        p = NULL;
                }
                if (ptype != objects::CSeq_id::e_Other)
                    p = NULL;
            }
            else
                p = NULL;

            if(p == NULL)
            {
                ErrPostEx(SEV_ERROR, ERR_SPROT_DRLine,
                          "Incorrect protein accession.version is used in RefSeq DR line: \"%s\". Skipped...",
                          token2);
                continue;
            }

            if(!AddToList(&pid_list, token2))
                continue;

            *p++ = '\0';
            CRef<objects::CSeq_id> id(MakeAccSeqId(token2, ptype, true, (Int2)atoi(p), false, false));
            if (id.NotEmpty())
                spb.SetSeqref().push_back(id);
        }
        else
        {
            if(StringICmp(token1, "GK") == 0)
                token1 = (char*) "Reactome";
            else if(StringICmp(token1, "GENEW") == 0)
                token1 = (char*) "HGNC";
            else if(StringICmp(token1, "GeneDB_Spombe") == 0)
                token1 = (char*) "PomBase";
            else if(StringICmp(token1, "PomBase") == 0 &&
                    StringNICmp(token2, "PomBase:", 8) == 0)
                token2 += 8;

            CRef<objects::CDbtag> tag = MakeStrDbtag(token1, token2);
            if (tag.NotEmpty())
            {
                bool not_found = true;

                ITERATE(objects::CSP_block::TDbref, cur_tag, spb.SetDbref())
                {
                    if (tag->Match(*(*cur_tag)))
                    {
                        not_found = false;
                        break;
                    }
                }
                if (not_found)
                    spb.SetDbref().push_back(tag);
            }
        }
    }

    if(embl_acc_list->next != NULL)
    {
        if(check_embl_prot)
            fta_check_embl_drxref_dups(embl_acc_list->next);
        ValNodeFreeData(embl_acc_list->next);
    }
    MemFree(embl_acc_list);

    if(acc_list != NULL)
        ValNodeFreeData(acc_list);
    if(pid_list != NULL)
        ValNodeFreeData(pid_list);
    if(ens_tran_list != NULL)
        ValNodeFreeData(ens_tran_list);
    if(ens_prot_list != NULL)
        ValNodeFreeData(ens_prot_list);
    if(ens_gene_list != NULL)
        ValNodeFreeData(ens_gene_list);
    MemFree(str);

    if(pdbold && pdbnew)
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_MixedPDBXrefs,
                  "Both old and new types of PDB cross-references exist on this record. Only one style is allowed.");
        *drop = 1;
    }

    if (pdbnew && spb.SetSeqref().size() > 1)
        CheckSPDupPDBXrefs(spb.SetSeqref());
}

/**********************************************************
 *
 *   static bool GetSPDate(pp, entry, crdate, sequpd,
 *                            annotupd, ver_num):
 *
 *      Contain three lines in order created, last sequence
 *   update, last annotation update.
 *
 *                                              9-30-93
 *
 **********************************************************/
static bool GetSPDate(ParserPtr pp, DataBlkPtr entry, objects::CDate& crdate,
                      objects::CDate& sequpd, objects::CDate& annotupd,
                      short* ver_num)
{
    ValNodePtr vnp;
    ValNodePtr tvnp;
    char*    offset;
    char*    p;
    char*    q;
    bool       new_style;
    bool       ret;
    Char       ch;
    Int4       first;
    Int4       second;
    Int4       third;
    size_t     len;

    CRef<objects::CDate_std> std_crdate,
                                         std_sequpd,
                                         std_annotupd;

    if(ver_num != NULL)
        *ver_num = 0;

    offset = SrchNodeType(entry, ParFlatSP_DT, &len);
    if(offset == NULL)
        return true;

    ch = offset[len];
    offset[len] = '\0';
    vnp = ValNodeNew(NULL);
    for(q = offset, tvnp = vnp;;)
    {
        p = StringChr(q, '\n');
        if(p == q)
            break;
        if(p != NULL)
            *p = '\0';
        tvnp->next = ValNodeNew(NULL);
        tvnp = tvnp->next;
        tvnp->data.ptrvalue = StringSave(q);
        if(p == NULL)
            break;
        *p++ = '\n';
        q = p;
        if(*q == '\0')
            break;
    }
    offset[len] = ch;
    tvnp = vnp->next;
    vnp->next = NULL;
    MemFree(vnp);
    vnp = tvnp;

    first = 0;
    second = 0;
    third = 0;
    if(StringChr((char*) vnp->data.ptrvalue, '(') == NULL)
    {
        new_style = true;
        for(tvnp = vnp; tvnp != NULL; tvnp = tvnp->next)
        {
            offset = (char*) tvnp->data.ptrvalue;
            offset += ParFlat_COL_DATA_SP;
            if(StringIStr(offset, "integrated into") != NULL)
            {
                first++;
                std_crdate = GetUpdateDate(offset, pp->source);
            }
            else if(StringIStr(offset, "entry version") != NULL)
            {
                third++;
                std_annotupd = GetUpdateDate(offset, pp->source);
            }
            else
            {
                p = StringIStr(offset, "sequence version");
                if(p != NULL)
                {
                    second++;
                    std_sequpd = GetUpdateDate(offset, pp->source);
                    if(ver_num != NULL)
                    {
                        for(p += 16; *p == ' ';)
                            p++;
                        for(q = p; *p >= '0' && *p <= '9';)
                            p++;
                        if(*p == '.' && p[1] == '\0')
                        {
                            *p = '\0';
                            *ver_num = atoi(q);
                            *p = '.';
                        }
                    }
                }
            }
        }
    }
    else
    {
        new_style = false;
        for(tvnp = vnp; tvnp != NULL; tvnp = tvnp->next)
        {
            offset = (char*) tvnp->data.ptrvalue;
            offset += ParFlat_COL_DATA_SP;
            if(StringIStr(offset, "Created") != NULL)
            {
                first++;
                std_crdate = GetUpdateDate(offset, pp->source);
            }
            else if(StringIStr(offset, "Last annotation update") != NULL)
            {
                third++;
                std_annotupd = GetUpdateDate(offset, pp->source);
            }
            else if(StringIStr(offset, "Last sequence update") != NULL)
            {
                second++;
                std_sequpd = GetUpdateDate(offset, pp->source);
            }
        }
    }

    ValNodeFreeData(vnp);

    ret = true;
    if(first == 0)
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_Date,
                  "Missing required \"%s\" DT line.",
                  (new_style ? "integrated into" : "Created"));
        ret = false;
    }
    else if(first > 1)
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_Date,
                  "Multiple \"%s\" DT lines are present.",
                  (new_style ? "integrated into" : "Created"));
        ret = false;
    }
    else if(second == 0)
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_Date,
                  "Missing required \"%s\" DT line.",
                  (new_style ? "sequence version" : "Last sequence update"));
        ret = false;
    }
    else if(second > 1)
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_Date,
                  "Multiple \"%s\" DT lines are present.",
                  (new_style ? "sequence version" : "Last sequence update"));
        ret = false;
    }
    else if(third == 0)
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_Date,
                  "Missing required \"%s\" DT line.",
                  (new_style ? "entry version" : "Last annotation update"));
        ret = false;
    }
    else if(third > 1)
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_Date,
                  "Multiple \"%s\" DT lines are present.",
                  (new_style ? "entry version" : "Last annotation update"));
        ret = false;
    }
    else if (std_crdate.Empty())
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_Date,
                  "Missing or incorrect create date in \"%s\" DT line.",
                  (new_style ? "integrated into" : "Created"));
        ret = false;
    }
    else if (std_sequpd.Empty())
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_Date,
                  "Missing or incorrect update date in \"%s\" DT line.",
                  (new_style ? "sequence version" : "Last sequence update"));
        ret = false;
    }
    else if (std_annotupd.Empty())
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_Date,
                  "Missing or incorrect update date in \"%s\" DT line.",
                  (new_style ? "entry version" : "Last annotation update"));
        ret = false;
    }
    else if(ver_num != NULL && *ver_num < 1)
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_Date,
                  "Invalidly formatted sequence version DT line is present.");
        ret = false;
    }

    if (ret)
    {
        crdate.SetStd(*std_crdate);
        sequpd.SetStd(*std_sequpd);
        annotupd.SetStd(*std_annotupd);
        return true;
    }

    return false;
}

/**********************************************************
 *
 *   static SPBlockPtr GetDescrSPBlock(pp, entry, bsp):
 *
 *                                              9-30-93
 *
 **********************************************************/
static CRef<objects::CSP_block>
    GetDescrSPBlock(ParserPtr pp, DataBlkPtr entry, objects::CBioseq& bioseq)
{
    IndexblkPtr  ibp;

    CRef<objects::CSP_block> spb(new objects::CSP_block);

    char*      bptr;
    bool         reviewed;
    bool         i;
    Int2         ver_num;

    /* first ID line, 2nd token
     */
    bptr = PointToNextToken(entry->offset + ParFlat_COL_DATA_SP);
    reviewed = (StringNICmp(bptr, "reviewed", 8) == 0);
    if (reviewed || StringNICmp(bptr, "standard", 8) == 0)
    {
        spb->SetClass(objects::CSP_block::eClass_standard);
    }
    else if(StringNICmp(bptr, "preliminary", 11) == 0 ||
            StringNICmp(bptr, "unreviewed", 10) == 0)
    {
        spb->SetClass(objects::CSP_block::eClass_prelim);
    }
    else
    {
        spb->SetClass(objects::CSP_block::eClass_not_set);
        ErrPostStr(SEV_WARNING, ERR_DATACLASS_UnKnownClass,
                   "Not a standard/reviewed or preliminary/unreviewed class in SWISS-PROT");
    }

    GetSequenceOfKeywords(entry, ParFlatSP_KW, ParFlat_COL_DATA_SP, spb->SetKeywords());

    ibp = pp->entrylist[pp->curindx];
    ibp->wgssec[0] = '\0';

    GetExtraAccession(ibp, pp->allow_uwsec, pp->source, spb->SetExtra_acc());
    if (spb->SetExtra_acc().empty())
        spb->ResetExtra_acc();

    /* DT data ==> create-date, seqence update, annotation update
     */
    ver_num = 0;
    if(reviewed && pp->sp_dt_seq_ver)
        i = GetSPDate(pp, entry, spb->SetCreated(), spb->SetSequpd(), spb->SetAnnotupd(), &ver_num);
    else
        i = GetSPDate(pp, entry, spb->SetCreated(), spb->SetSequpd(), spb->SetAnnotupd(), NULL);

    get_plasmid(entry, spb->SetPlasnm());
    if (spb->SetPlasnm().empty())
        spb->ResetPlasnm();

    GetDRlineDataSP(entry, *spb, &ibp->drop, pp->source);

    if (!i)
        ibp->drop = 1;
    else if (spb->GetClass() == objects::CSP_block::eClass_standard ||
             spb->GetClass() == objects::CSP_block::eClass_prelim)
    {
        NON_CONST_ITERATE(objects::CBioseq::TId, cur_id, bioseq.SetId())
        {
            if (!(*cur_id)->IsSwissprot())
                continue;

            objects::CTextseq_id& id = (*cur_id)->SetSwissprot();
            if(ver_num > 0)
                id.SetVersion(ver_num);

            if (spb->GetClass() == objects::CSP_block::eClass_standard)
                id.SetRelease("reviewed");
            else
                id.SetRelease("reviewed");

            break;
        }
    }

    CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
    descr->SetSp(*spb);
    bioseq.SetDescr().Set().push_back(descr);

    return spb;
}

/**********************************************************
 *
 *   static void ParseSpComment(descrptr, bptr, eptr):
 *
 *                                              10-1-93
 *
 **********************************************************/
static void ParseSpComment(objects::CSeq_descr::Tdata& descrs, char* line)
{
    char* com;
    char* p;
    char* q;
    Int2    i;

    for(p = line; *p == ' ';)
        p++;

    com = (char*) MemNew(StringLen(p) + 2);
    q = com;
    i = fta_StringMatch(ParFlat_SPComTopics, p);
    if(i >= 0)
        *q++ = '[';

    while(*p != '\0')
    {
        if(*p != '\n')
        {
            *q++ = *p++;
            continue;
        }

        if(p > line && *(p - 1) != '-')
            *q++ = ' ';
        for(++p; *p == ' ';)
            p++;
        if(StringNCmp(p, "CC ", 3) == 0)
            for(p += 3; *p == ' ';)
                p++;
    }
    if(q == com)
    {
        MemFree(com);
        return;
    }
    for(--q; q > com && *q == ' ';)
        q--;
    if(*q != ' ')
        q++;
    *q = '\0';
    if(i >= 0)
    {
        p = StringChr(com, ':');
        *p = ']';
    }

    if (com[0] != 0)
    {
        CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
        descr->SetComment(com);
        descrs.push_back(descr);
    }
    MemFree(com);
}

/**********************************************************
 *
 *   static void GetSPDescrComment(entry, descrs, acc, cla):
 *
 *      CC line ==> comment, separate each by "-!-".
 *
 *                                              10-1-93
 *
 **********************************************************/
static void GetSPDescrComment(DataBlkPtr entry, objects::CSeq_descr::Tdata& descrs,
                              char* acc, Uint1 cla)
{
    char* offset;
    char* bptr;
    char* eptr;
    char* tmp;
    char* p;
    char* q;
    Char    ch;
    Int2    count;
    Int4    i;

    size_t len = 0;
    offset = SrchNodeType(entry, ParFlatSP_CC, &len);
    if(offset == NULL)
        return;

    eptr = offset + len;
    ch = *eptr;
    *eptr = '\0';
    for(count = 0, p = offset;;)
    {
        p = StringStr(p, "----------");
        if(p == NULL)
             break;
        for(q = p; q > offset && *q != '\n';)
            q--;
        if(*q == '\n')
            q++;

        p = StringChr(p, '\n');
        if(p == NULL)
            break;
        for(i = 0; *p != '\0' && i < ParFlat_COL_DATA_SP + 1; i++)
            p++;
        if(*p == '\0')
            break;
        if(StringNICmp(p, COPYRIGHT, StringLen(COPYRIGHT)) != 0 &&
           StringNICmp(p, COPYRIGHT1, StringLen(COPYRIGHT1)) != 0)
            break;
        p = StringStr(p, "----------");
        if(p == NULL)
            break;
        p = StringChr(p, '\n');
        if(p == NULL)
            break;
        p++;
        len -= (p - q);
        fta_StringCpy(q, p);
        p = q;
        count++;
    }

    if(count == 0 && cla != 2)          /* not PRELIMINARY or UNREVIEWED */
        ErrPostEx(SEV_WARNING, ERR_FORMAT_MissingCopyright,
                  "The expected copyright notice for UniProt/Swiss-Prot entry %s was not found.",
                  acc);

    if(len < 1)
    {
        *eptr = ch;
        return;
    }

    bptr = offset + ParFlat_COL_DATA_SP + 4;

    for(; (tmp = StringStr(bptr, "-!-")) != NULL; bptr = tmp + 4)
    {
        /* found a new comment
         */
        for(p = tmp; p > bptr && *p != '\n';)
            p--;
        if(p == bptr)
            continue;
        *p = '\0';
        ParseSpComment(descrs, bptr);
        *p = '\n';
    }

    ParseSpComment(descrs, bptr);
    *eptr = ch;
}

/**********************************************************/
static void SPAppendPIRToHist(objects::CBioseq& bioseq, const objects::CSP_block& spb)
{
    if (spb.GetSeqref().empty())
        return;

    objects::CSeq_hist_rec::TIds rep_ids;

    ITERATE(objects::CSP_block::TSeqref, cur_ref, spb.GetSeqref())
    {
        if (!(*cur_ref)->IsPir())
            continue;

        CRef<objects::CTextseq_id> text_id(new objects::CTextseq_id);
        text_id->Assign((*cur_ref)->GetPir());

        CRef<objects::CSeq_id> rep_id(new objects::CSeq_id);
        rep_id->SetPir(*text_id);

        rep_ids.push_back(rep_id);
    }

    if (rep_ids.empty())
        return;

    objects::CSeq_hist& hist = bioseq.SetInst().SetHist();
    hist.SetReplaces().SetIds().splice(hist.SetReplaces().SetIds().end(), rep_ids);
}

/**********************************************************/
static bool IfOHTaxIdMatchOHName(char* orpname, char* ohname)
{
    char* p;
    char* q;
    Char    chp;
    Char    chq;

    if(orpname == NULL && ohname == NULL)
        return true;
    if(orpname == NULL || ohname == NULL)
        return false;

    for(p = orpname, q = ohname; *p != '\0' && *q != '\0'; p++, q++)
    {
        chp = *p;
        if(chp >= 'a' && chp <= 'z')
            chp &= ~040;
        chq = *q;
        if(chq >= 'a' && chq <= 'z')
            chq &= ~040;
        if(chp != chq)
            break;
    }

    while(*p == ' ')
        p++;
    if(*p != '\0')
        return false;

    while(*q == ' ')
        q++;
    if(*q == '(' || *q == '\0')
        return true;
    return false;
}

/**********************************************************/
static void GetSprotDescr(objects::CBioseq& bioseq, ParserPtr pp, DataBlkPtr entry)
{
    DataBlkPtr   dbp;
    char*      offset;
    Uint1        gmod;
    bool         fragment = false;
    Int4         taxid;

    IndexblkPtr  ibp;
    ViralHostPtr vhp;
    ViralHostPtr tvhp;

    objects::CSeq_descr& descr = bioseq.SetDescr();

    ibp = pp->entrylist[pp->curindx];
    size_t len = 0;
    offset = SrchNodeType(entry, ParFlatSP_DE, &len);
    if (offset != NULL)
    {
        char* title = GetSPDescrTitle(offset, offset + len, &fragment);
        if (title != NULL)
        {
            CRef<objects::CSeqdesc> desc_new(new objects::CSeqdesc);
            desc_new->SetTitle(title);
            descr.Set().push_back(desc_new);
        }
    }

    /* sp-block
     */
    CRef<objects::CSP_block> spb = GetDescrSPBlock(pp, entry, bioseq);

    GetSPDescrComment(entry, descr.Set(), ibp->acnum, spb->GetClass());

    if (spb.NotEmpty() && pp->accver && pp->histacc && pp->source == Parser::ESource::SPROT)
    {
        objects::CSeq_hist_rec::TIds rep_ids;

        ITERATE(objects::CSP_block::TExtra_acc, cur_acc, spb->GetExtra_acc())
        {
            if (cur_acc->empty() || !IsSPROTAccession(cur_acc->c_str()))
                continue;

            CRef<objects::CTextseq_id> text_id(new objects::CTextseq_id);
            text_id->SetAccession(*cur_acc);

            CRef<objects::CSeq_id> rep_id(new objects::CSeq_id);
            rep_id->SetSwissprot(*text_id);
            rep_ids.push_back(rep_id);
        }

        if (!rep_ids.empty())
        {
            objects::CSeq_hist& hist = bioseq.SetInst().SetHist();
            hist.SetReplaces().SetIds().swap(rep_ids);
        }
    }

    if (spb->CanGetCreated())
    {
        CRef<objects::CSeqdesc> create_date_descr(new objects::CSeqdesc);
        create_date_descr->SetCreate_date().Assign(spb->GetCreated());

        descr.Set().push_back(create_date_descr);
    }

    bool has_update_date = spb->CanGetAnnotupd() || spb->CanGetSequpd();
    objects::CDate upd_date;
    
    if (has_update_date)
    {
        if (spb->CanGetAnnotupd() && spb->CanGetSequpd())
        {
            upd_date.Assign(spb->GetAnnotupd().Compare(spb->GetSequpd()) == objects::CDate::eCompare_after ?
                spb->GetAnnotupd() : spb->GetSequpd());
        }
        else if (spb->CanGetAnnotupd())
            upd_date.Assign(spb->GetAnnotupd());
        else
            upd_date.Assign(spb->GetSequpd());

        CRef<objects::CSeqdesc> upd_date_descr(new objects::CSeqdesc);
        upd_date_descr->SetUpdate_date().Assign(upd_date);

        descr.Set().push_back(upd_date_descr);
    }

    if (spb->CanGetCreated() && has_update_date &&
        spb->GetCreated().Compare(upd_date) == objects::CDate::eCompare_after)
    {
        std::string upd_date_str,
                    create_date_str;

        upd_date.GetDate(&upd_date_str);
        spb->GetCreated().GetDate(&create_date_str);

        ErrPostEx(SEV_ERROR, ERR_DATE_IllegalDate,
                  "Update-date \"%s\" precedes create-date \"%s\".",
                  upd_date_str.c_str(),
                  create_date_str.c_str());
    }

    dbp = TrackNodeType(entry, ParFlatSP_OS);
    gmod = GetSPGenome(dbp);

    /* Org-ref from ID lines
     */
    for(dbp = TrackNodeType(entry, ParFlatSP_ID); dbp != NULL; dbp = dbp->next)
    {
        if(dbp->type != ParFlatSP_ID)
            continue;

        CRef<objects::CBioSource> bio_src;

        taxid = GetTaxIdFrom_OX(dbp);
        if(taxid > 0)
        {
            CRef<objects::COrg_ref> org_ref = fta_fix_orgref_byid(pp, taxid, &ibp->drop, false);
            if (org_ref.Empty())
                ErrPostEx(SEV_ERROR, ERR_SOURCE_NcbiTaxIDLookupFailure,
                          "NCBI TaxID lookup for %d failed : will use organism name for lookup instead.",
                          taxid);
            else
            {
                bio_src.Reset(new objects::CBioSource);

                if (gmod != objects::CBioSource::eGenome_unknown)
                    bio_src->SetGenome(gmod);
                bio_src->SetOrg(*org_ref);
            }
        }

        CRef<objects::COrg_ref> org_ref = GetOrganismFrom_OS_OC(dbp);
        if (org_ref.NotEmpty())
        {
            if (bio_src.Empty())
            {
                bio_src.Reset(new objects::CBioSource);

                if (gmod != objects::CBioSource::eGenome_unknown)
                    bio_src->SetGenome(gmod);
                fta_fix_orgref(pp, *org_ref, &ibp->drop, NULL);
                bio_src->SetOrg(*org_ref);
            }
            else if (org_ref->IsSetTaxname())
            {
                if (!bio_src->IsSetOrg() || !bio_src->GetOrg().IsSetTaxname() ||
                    StringICmp(org_ref->GetTaxname().c_str(), bio_src->GetOrg().GetTaxname().c_str()) != 0)
                    ErrPostEx(SEV_ERROR, ERR_SOURCE_OrgNameVsTaxIDMissMatch,
                              "Organism name \"%s\" from OS line does not match the organism name \"%s\" obtained by lookup of NCBI TaxID.",
                              org_ref->GetTaxname().c_str(), bio_src->GetOrg().GetTaxname().c_str());
            }
        }

        if (bio_src.Empty())
            break;

        vhp = GetViralHostsFrom_OH(dbp);
        if(vhp != NULL)
        {
            objects::COrgName& orgname = bio_src->SetOrg().SetOrgname();

            for(tvhp = vhp; tvhp != NULL; tvhp = vhp)
            {
                vhp = tvhp->next;

                CRef<objects::COrgMod> mod(new objects::COrgMod);
                mod->SetSubtype(21);
                mod->SetSubname(tvhp->name);
                orgname.SetMod().push_back(mod);

                if(tvhp->taxid < 1)
                {
                    MemFree(tvhp);
                    continue;
                }

                gmod = 0;

                CRef<objects::COrg_ref> org_ref_cur = fta_fix_orgref_byid(pp, tvhp->taxid, &gmod, true);
                if (org_ref_cur.Empty())
                {
                    if(gmod == 0)
                        ErrPostEx(SEV_ERROR, ERR_SOURCE_InvalidNcbiTaxID,
                                  "OH-line TaxId \"%d\" was not found via the NCBI TaxArch service.",
                                  tvhp->taxid);
                    else
                        ErrPostEx(SEV_ERROR, ERR_SOURCE_NcbiTaxIDLookupFailure,
                                  "Taxonomy lookup for OH-line TaxId \"%d\" failed.",
                                  tvhp->taxid);
                }
                else
                {
                    std::vector<Char> org_taxname;
                    if (org_ref_cur->IsSetTaxname())
                    {
                        const std::string& cur_taxname = org_ref_cur->GetTaxname();
                        org_taxname.assign(cur_taxname.begin(), cur_taxname.end());
                    }

                    org_taxname.push_back(0);

                    if (!IfOHTaxIdMatchOHName(&org_taxname[0], tvhp->name))
                        ErrPostEx(SEV_WARNING,
                                  ERR_SOURCE_HostNameVsTaxIDMissMatch,
                                  "OH-line HostName \"%s\" does not match NCBI organism name \"%s\" obtained by lookup of NCBI TaxID \"%d\".",
                                  tvhp->name, &org_taxname[0], tvhp->taxid);
                }
                MemFree(tvhp);
            }
        }

        fta_sort_biosource(*bio_src);

        CRef<objects::CSeqdesc> bio_src_desc(new objects::CSeqdesc);
        bio_src_desc->SetSource(*bio_src);
        descr.Set().push_back(bio_src_desc);
        break;
    }

    if (spb.NotEmpty())
        SPAppendPIRToHist(bioseq, *spb);

    CRef<objects::CSeqdesc> mol_info_descr(new objects::CSeqdesc);
    objects::CMolInfo& mol_info = mol_info_descr->SetMolinfo();
    mol_info.SetBiomol(objects::CMolInfo::eBiomol_peptide);
    mol_info.SetCompleteness(fragment ? objects::CMolInfo::eCompleteness_partial : objects::CMolInfo::eCompleteness_complete);

    descr.Set().push_back(mol_info_descr);

    /* RN data ==> pub
     */
    dbp = TrackNodeType(entry, ParFlat_REF_END);
    for(; dbp != NULL; dbp = dbp->next)
    {
        if(dbp->type != ParFlat_REF_END)
            continue;

        CRef<objects::CPubdesc> pub_desc = DescrRefs(pp, dbp, ParFlat_COL_DATA_SP);
        if (pub_desc.NotEmpty())
        {
            CRef<objects::CSeqdesc> pub_desc_descr(new objects::CSeqdesc);
            pub_desc_descr->SetPub(*pub_desc);

            descr.Set().push_back(pub_desc_descr);
        }
    }
}

/**********************************************************
 *
 *   static void GetSPInst(pp, entry, protconv):
 *
 *      Fills in Seq-inst for an entry. Assumes Bioseq
 *   already allocated.
 *
 *                                              10-8-93
 *
 **********************************************************/
static void GetSPInst(ParserPtr pp, DataBlkPtr entry, unsigned char* protconv)
{
    EntryBlkPtr ebp;

    ebp = (EntryBlkPtr) entry->data;

    objects::CBioseq& bioseq = ebp->seq_entry->SetSeq();

    bioseq.SetInst().SetRepr(objects::CSeq_inst::eRepr_raw);
    bioseq.SetInst().SetMol(objects::CSeq_inst::eMol_aa);

    GetSeqData(pp, entry, bioseq, ParFlatSP_SQ, protconv, objects::CSeq_data::e_Iupacaa);
}

/**********************************************************/
static void FreeSPFeatInput(SPFeatInputPtr spfip)
{
    delete spfip;
}

/**********************************************************
 *
 *   static void FreeSPFeatInputSet(spfip):
 *
 *                                              10-18-93
 *
 **********************************************************/
static void FreeSPFeatInputSet(SPFeatInputPtr spfip)
{
    SPFeatInputPtr next;

    for(; spfip != NULL; spfip = next)
    {
        next = spfip->next;
        FreeSPFeatInput(spfip);
    }
}

/**********************************************************/
static bool fta_spfeats_same(SPFeatInputPtr fip1, SPFeatInputPtr fip2)
{
    if(fip1 == NULL && fip2 == NULL)
        return true;

    if(fip1 == NULL || fip2 == NULL ||
       fip1->key != fip2->key ||
       fip1->from != fip2->from ||
       fip1->to != fip2->to ||
       fip1->descrip != fip2->descrip)
        return false;

    return true;
}

/**********************************************************/
static void fta_remove_dup_spfeats(SPFeatInputPtr spfip)
{
    SPFeatInputPtr fip;
    SPFeatInputPtr fipnext;
    SPFeatInputPtr fipprev;

    if(spfip == NULL || spfip->next == NULL)
        return;

    for(; spfip != NULL && spfip->next != NULL; spfip = spfip->next)
    {
        fipprev = spfip;
        for(fip = spfip->next; fip != NULL; fip = fipnext)
        {
            fipnext = fip->next;
            if(!fta_spfeats_same(spfip, fip))
            {
                fipprev = fip;
                continue;
            }
            fipprev->next = fip->next;
            ErrPostEx(SEV_WARNING, ERR_FEATURE_DuplicateRemoved,
                      "Duplicated feature \"%s\" at location \"%s..%s\" removed.",
                      fip->key.empty() ? "???" : fip->key.c_str(),
                      fip->from.empty() ? "???" : fip->from.c_str(),
                      fip->to.empty() ? "???" : fip->to.c_str());
            FreeSPFeatInput(fip);
        }
    }
}

/**********************************************************/
static void SPPostProcVarSeq(std::string& varseq)
{
    char* temp;
    char* end;
    char* p;
    char* q;

    if (varseq.empty())
        return;

    temp = StringSave(varseq.c_str());
    p = StringStr(temp, "->");
    if(p == NULL || p == temp ||
       (*(p - 1) != ' ' && *(p - 1) != '\n') || (p[2] != ' ' && p[2] != '\n'))
    {
        NStr::ReplaceInPlace(varseq, "\n", " ");
        MemFree(temp);
        return;
    }

    for(p--; p > temp && (*p == ' ' || *p == '\n');)
        p--;
    if(*p < 'A' || *p > 'Z')
    {
        NStr::ReplaceInPlace(varseq, "\n", " ");
        MemFree(temp);
        return;
    }

    end = p + 1;
    while(p > temp && (*p == '\n' || (*p >= 'A' && *p <= 'Z')))
        p--;
    if(p > temp)
        p++;
    while(*p == '\n')
        p++;
    for(;;)
    {
        while(*p >= 'A' && *p <= 'Z' && p < end)
            p++;
        if(p == end)
            break;
        for(q = p; *p == '\n'; p++)
            end--;
        fta_StringCpy(q, p);
    }

    while(*p == ' ' || *p == '\n')
        p++;
    for(p += 2; *p == ' ' || *p == '\n';)
        p++;

    if(*p < 'A' || *p > 'Z')
    {
        NStr::ReplaceInPlace(varseq, "\n", " ");
        MemFree(temp);
        return;
    }

    for(q = p; *q == '\n' || (*q >= 'A' && *q <= 'Z');)
        q++;
    if(q > p && *(q - 1) == '\n')
    {
        for(q--; *q == '\n' && q > p;)
            q--;
        if(*q != '\n')
            q++;
    }
    end = q;

    for(;;)
    {
        while(*p >= 'A' && *p <= 'Z' && p < end)
            p++;
        if(p == end)
            break;
        for(q = p; *p == '\n'; p++)
            end--;
        fta_StringCpy(q, p);
    }

    for(p = temp; *p != '\0'; p++)
        if(*p == '\n')
            *p = ' ';

    varseq = temp;
    MemFree(temp);
}

/**********************************************************
 *
 *   static SPFeatInputPtr ParseSPFeat(entry, seqlen):
 *
 *      Return a link list of feature input data, including
 *   key, from, to, description.
 *
 *                                              10-15-93
 *
 **********************************************************/
static SPFeatInputPtr ParseSPFeat(DataBlkPtr entry, size_t seqlen)
{
    SPFeatInputPtr temp;
    SPFeatInputPtr current;
    SPFeatInputPtr spfip;
    const char     *defdelim;
    char*        fromstart;
    char*        fromend;
    char*        bptr;
    char*        eptr;
    char*        ptr1;
    char*        offset;
    char*        endline;
    char*        str;
    char*        delim;
    char*        quotes;
    char*        location;
    char*        p;
    char*        q;
    int            i;
    bool           badqual;
    bool           new_format;
    bool           extra_text;
    Char           ch;

    size_t len = 0;
    offset = SrchNodeType(entry, ParFlatSP_FT, &len);
    if(offset == NULL)
        return(NULL);

    bptr = offset + ParFlat_COL_DATA_SP;
    eptr = offset + len;

    spfip = NULL;
    current = NULL;

    while(bptr < eptr && (endline = SrchTheChar(bptr, eptr, '\n')) != NULL)
    {
        temp = new SPFeatInput;

        for(p = bptr, i = 0; *p != ' ' && *p != '\n' && i < 8; i++)
            p++;
        temp->key.assign(bptr, p);
        NStr::TruncateSpacesInPlace(temp->key, NStr::eTrunc_End);

        if (temp->key == "VAR_SEQ")
            defdelim = "\n";
        else
            defdelim = " ";

        for(bptr += 8; *bptr == ' ' && bptr <= endline;)
            bptr++;

        location = bptr;

        if(((*bptr >= 'a' && *bptr <= 'z') || (*bptr >= 'A' && *bptr <= 'Z')) &&
           bptr[6] == '-')
        {
            for(bptr += 7; *bptr >= '0' && *bptr <= '9' && bptr <= endline;)
                bptr++;
            for(; *bptr == ':' && bptr <= endline;)
                bptr++;
        }

        for(ptr1 = bptr; *ptr1 == '?' || *ptr1 == '>' || *ptr1 == '<' ||
                         (*ptr1 >= '0' && *ptr1 <= '9');)
            ptr1++;

        if(bptr < ptr1 && ptr1 <= endline)
        {
            temp->from.assign(bptr, ptr1);
            fromstart = bptr;
            fromend = ptr1;
        }
        else
        {
            ch = '\0';
            p = StringChr(location, ' ');
            q = StringChr(location, '\n');
            if(p == NULL || (q != NULL && q < p))
                p = q;
            if(p != NULL)
            {
                ch = *p;
                *p = '\0';
            }
            if(bptr == ptr1)
                ErrPostEx(SEV_ERROR, ERR_FEATURE_BadLocation,
                          "Invalid location \"%s\" at feature \"%s\". Feature dropped.",
                          location, temp->key.c_str());
            else
                ErrPostEx(SEV_ERROR, ERR_FEATURE_BadLocation,
                          "Empty location at feature \"%s\". Feature dropped.",
                          temp->key.c_str());
            if(p != NULL)
                *p = ch;
            temp->from.assign("-1");
            fromstart = NULL;
            fromend = NULL;
        }

        new_format = false;
        bptr = ptr1;
        for(; (*bptr == ' ' || *bptr == '.') && bptr <= endline; bptr++)
            if(*bptr == '.')
                new_format = true;
        for(ptr1 = bptr; *ptr1 == '?' || *ptr1 == '>' || *ptr1 == '<' ||
                         (*ptr1 >= '0' && *ptr1 <= '9');)
            ptr1++;

        p = (char *) temp->from.c_str();
        if(*p == '<' || *p == '>')
            p++;

        for(q = ptr1; *q == ' ';)
            q++;
        extra_text = false;
        if(bptr < ptr1 && ptr1 <= endline)
        {
            if(*q != '\n' && new_format && (*p == '?' || atoi(p) != -1))
                extra_text = true;
            temp->to.assign(bptr, ptr1);
        }
        else if(fromstart)
        {
            if(*q != '\n' && (*p == '?' || atoi(p) != -1))
                extra_text = true;
            temp->to.assign(fromstart, fromend);
        }
        else
        {
            if(*q != '\n' && (*p == '?' || atoi(p) != -1))
                extra_text = true;
            temp->to.assign("-1");
        }

        q = (char *) temp->to.c_str();
        if(*q == '<' || *q == '>')
            q++;
        if(extra_text || (*p != '?' && *q != '?' && (atoi(p) > atoi(q))))
        {
            ch = '\0';
            p = extra_text ? NULL : StringChr(location, ' ');
            q = StringChr(location, '\n');
            if(p == NULL || (q != NULL && q < p))
                p = q;
            if(p != NULL)
            {
                ch = *p;
                *p = '\0';
            }
            ErrPostEx(SEV_ERROR, ERR_FEATURE_BadLocation,
                      "Invalid location \"%s\" at feature \"%s\". Feature dropped.",
                      location, temp->key.c_str());
            if(p != NULL)
                *p = ch;
            temp->from.assign("-1");
        }

        for(bptr = ptr1; *bptr == ' ' && bptr <= endline;)
            bptr++;

        str = endline;
        delim = (char*) defdelim;
        if(str > bptr)
            if(*--str == '-' && str > bptr)
                if(*--str != ' ')
                    delim = NULL;
        if(bptr <= endline)
            temp->descrip.assign(bptr, endline);

        for(bptr = endline; *bptr == ' ' || *bptr == '\n';)
            bptr++;

        badqual = false;
        bptr += ParFlat_COL_DATA_SP;
        while(bptr < eptr && (*bptr == ' '))    /* continue description data */
        {
            while(*bptr == ' ')
                bptr++;

            if(!StringNCmp(bptr, "/note=\"", 7))
            {
                bptr += 7;
                quotes = NULL;
            }
            else if(!StringNCmp(bptr, "/evidence=\"", 11))
            {
                quotes = bptr + 10;
                if(StringNCmp(quotes + 1, "ECO:", 4) != 0)
                {
                    p = StringChr(bptr, '\n');
                    if(p != NULL)
                        *p = '\0';
                    ErrPostEx(SEV_ERROR, ERR_QUALIFIER_InvalidEvidence,
                              "/evidence qualifier does not have expected \"ECO:\" prefix : \"%s\".",
                              bptr);
                    if(p != NULL)
                        *p = '\n';
                }
            }
            else if(!StringNCmp(bptr, "/id=\"", 5))
                quotes = bptr + 4;
            else
            {
                if(*bptr == '/')
                {
                    for(p = bptr + 1; (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_';)
                        p++;
                    if(*p == '=' && p[1] == '\"')
                    {
                        *p = '\0';
                        badqual = true;
                        ErrPostEx(SEV_ERROR, ERR_FEATURE_InvalidQualifier,
                                  "Qualifier %s is invalid for the feature \"%s\" at \"%s..%s\".",
                                  bptr, temp->key.c_str(), temp->from.c_str(),
                                  temp->to.c_str());
                        *p = '=';
                    }

                }
                quotes = NULL;
            }

            endline = SrchTheChar(bptr, eptr, '\n');
            p = endline - 1;
            if(p >= bptr && *p == '\"')
                *p = '.';
            else
                p = NULL;

            if(quotes)
            {
                StringCombine(temp->descrip, std::string(bptr, quotes), delim);
                if(p && p - 1 >= bptr && *(p - 1) == '.')
                    StringCombine(temp->descrip, std::string(quotes + 1, endline - 1), "");
                else
                    StringCombine(temp->descrip, std::string(quotes + 1, endline), "");
            }
            else
            {
                if(p && p - 1 >= bptr && *(p - 1) == '.')
                    StringCombine(temp->descrip, std::string(bptr, endline - 1), delim);
                else
                    StringCombine(temp->descrip, std::string(bptr, endline), delim);
             }

            if(p)
                *p = '\"';

            str = endline;
            delim = (char*) defdelim;
            if(str > bptr)
                if(*--str == '-' && str > bptr)
                    if(*--str != ' ')
                        delim = NULL;
            for(bptr = endline; *bptr == ' ' || *bptr == '\n';)
                bptr++;

            bptr += ParFlat_COL_DATA_SP;
        }

        if (badqual)
        {
            ErrPostEx(SEV_ERROR, ERR_FEATURE_Dropped,
                      "Invalid qualifier(s) found within the feature \"%s\" at \"%s..%s\". Feature dropped.",
                      temp->key.c_str(), temp->from.c_str(), temp->to.c_str());
            FreeSPFeatInputSet(temp);
            continue;
        }

        if(*defdelim == '\n')
            SPPostProcVarSeq(temp->descrip);

        p = (char *) temp->from.c_str();
        if(*p == '<' || *p == '>')
            p++;
        if(*p != '?' && atoi(p) < 0)
        {
            FreeSPFeatInputSet(temp);
            continue;
        }

        q = (char *) temp->to.c_str();
        if(*q == '<' || *q == '>')
            q++;
        if((*p != '?' && atoi(p) > (Int4) seqlen) || (*q != '?' && atoi(q) > (Int4) seqlen))
        {
            ErrPostEx(SEV_WARNING, ERR_LOCATION_FailedCheck,
                      "Location range exceeds the sequence length: feature=%s, length=%d, from=%s, to=%s",
                      temp->key.c_str(), seqlen, temp->from.c_str(), temp->to.c_str());
            ErrPostEx(SEV_ERROR, ERR_FEATURE_Dropped,
                      "Location range exceeds the sequence length: feature=%s, length=%d, from=%s, to=%s",
                      temp->key.c_str(), seqlen, temp->from.c_str(), temp->to.c_str());
            FreeSPFeatInputSet(temp);
            continue;
        }

        if(spfip == NULL)
            spfip = temp;
        else
            current->next = temp;
        current = temp;
    }

    fta_remove_dup_spfeats(spfip);

    return(spfip);
}

/**********************************************************
 *
 *   static CRef<objects::CSeq_loc> GetSPSeqLoc(pp, spfip, bond, initmet,
 *                                signal):
 *
 *      The following rules are assumption since I am
 *   waiting Mark's mail:
 *      - substract one if from > 0 and
 *      - unknown endpoint "?" not implement.
 *
 *                                              10-18-93
 *
 **********************************************************/
static CRef<objects::CSeq_loc> GetSPSeqLoc(ParserPtr pp, SPFeatInputPtr spfip,
                                                       bool bond, bool initmet, bool signal)
{
    CRef<objects::CSeq_loc> loc;

    IndexblkPtr ibp;

    const Char* ptr;

    bool        fuzzfrom = false;
    bool        fuzzto = false;
    bool        nofrom = false;
    bool        noto = false;
    bool        pntfuzz = false;
    Int4        from;
    Int4        to;

    if(spfip == NULL || spfip->from == NULL || spfip->to == NULL)
        return loc;

    ibp = pp->entrylist[pp->curindx];

    loc.Reset(new objects::CSeq_loc);

    ptr = spfip->from.c_str();
    if(StringChr(ptr, '<') != NULL)
    {
        fuzzfrom = true;

        while(*ptr != '\0' && IS_DIGIT(*ptr) == 0)
            ptr++;
        from = (Int4) atoi(ptr);
    }
    else if(StringChr(ptr, '?') != NULL)
    {
        from = 0;
        nofrom = true;
    }
    else
    {
        from = (Int4) atoi(ptr);
    }
    if((initmet == false && from != 0) ||
       (initmet && signal && from == 1))
        from--;

    ptr = spfip->to.c_str();
    if(StringChr(ptr, '>') != NULL)
    {
        fuzzto = true;
        while(*ptr != '\0' && IS_DIGIT(*ptr) == 0)
            ptr++;
        to = (Int4) atoi(ptr);
    }
    else if(StringChr(ptr, '?') != NULL)
    {
        to = static_cast<Int4>(ibp->bases);
        noto = true;
    }
    else
        to = (Int4) atoi(ptr);

    if(initmet == false && to != 0)
        to--;
    if(nofrom && noto)
        pntfuzz = true;

    if (bond)
    {
        objects::CSeq_bond& bond = loc->SetBond();
        objects::CSeq_point& point_a = bond.SetA();

        point_a.SetPoint(from);
        point_a.SetId(*MakeAccSeqId(ibp->acnum, pp->seqtype, pp->accver, ibp->vernum, false, false));

        if (fuzzfrom)
            GetIntFuzzPtr(4, 2, 0, point_a.SetFuzz());

        if (from != to)
        {
            objects::CSeq_point& point_b = bond.SetB();
            point_b.SetPoint(to);
            point_b.SetId(*MakeAccSeqId(ibp->acnum, pp->seqtype, pp->accver, ibp->vernum, false, false));

            if(fuzzto)
                GetIntFuzzPtr(4, 1, 0, point_b.SetFuzz());
        }
    }
    else if(from != to && !pntfuzz)
    {
        objects::CSeq_interval& interval = loc->SetInt();
        interval.SetFrom(from);
        interval.SetTo(to);
        interval.SetId(*MakeAccSeqId(ibp->acnum, pp->seqtype, pp->accver, ibp->vernum, false, false));

        if(fuzzfrom)
            GetIntFuzzPtr(4, 2, 0, interval.SetFuzz_from());      /* lim, lt, no-min */

        if(nofrom)
            GetIntFuzzPtr(2, to - 1, 0, interval.SetFuzz_from()); /* range, max, min */

        if(noto)
            GetIntFuzzPtr(2, to, from + 1, interval.SetFuzz_to()); /* range, max, min */

        if(fuzzto)
            GetIntFuzzPtr(4, 1, 0, interval.SetFuzz_to());        /* lim, gt, no-min */
    }
    else
    {
        objects::CSeq_point& point = loc->SetPnt();
        point.SetPoint(from);
        point.SetId(*MakeAccSeqId(ibp->acnum, pp->seqtype, pp->accver, ibp->vernum, false, false));

        if (pntfuzz)
        {
            GetIntFuzzPtr(2, to, from, point.SetFuzz());     /* range, max, min */
        }
        else if(fuzzfrom)
        {
            GetIntFuzzPtr(4, 2, 0, point.SetFuzz());
        }
    }

    return loc;
}

/**********************************************************
 *
 *   static char* DelTheStr(sourcesrt, targetstr):
 *
 *      Return a string with deleted "targetstr".
 *   Also Free out "sourcestr".
 *
 **********************************************************/
/* bsv : 03/04/2020 : no Seq-feat.exp-ev setting anymore
static void DelTheStr(std::string& sourcestr, const std::string& targetstr)
{
    NStr::ReplaceInPlace(sourcestr, targetstr, "", 0, 1);
    NStr::TruncateSpacesInPlace(sourcestr, NStr::eTrunc_End);
}
*/

/**********************************************************
 *
 *   static bool SPFeatNoExp(pp, spfip):
 *
 *      Return TRUE if "str" containing any string in the
 *   ParFlat_SPFeatNoExp or ParFlat_SPFeatNoExpW (old
 *   patterns, put warning message).
 *
 *                                              10-18-93
 *
 **********************************************************/
/* bsv : 03/04/2020 : no Seq-feat.exp-ev setting anymore
static bool SPFeatNoExp(ParserPtr pp, SPFeatInputPtr spfip)
{
    Int2    indx;
    Int4    len = 0;

    if(spfip == NULL)
        return false;

    if(MatchArrayISubString(ParFlat_SPFeatNoExp, spfip->descrip.c_str()) != -1)
        return true;

    indx = MatchArrayISubString(ParFlat_SPFeatNoExpW, spfip->descrip.c_str());
    if(indx == -1)
        return false;

    DelTheStr(spfip->descrip, ParFlat_SPFeatNoExpW[indx]);
    if (len > 0 && spfip->descrip[len-1] != '.')
    {
        StringCombine(spfip->descrip, ".", NULL);
    }

    ErrPostEx(SEV_WARNING, ERR_FEATURE_OldNonExp,
              "Old Non-experimental feature description, %s",
              ParFlat_SPFeatNoExpW[indx]);

    return true;
}
*/

/**********************************************************
 *
 *   static Int2 GetSPSitesMod(retstr):
 *
 *      Return an index array of ParFlat_SPFEAT for
 *   a specific type of modified residue because the first
 *   several words of a MOD_RES feature's description can
 *   indicate a more specific type of modified residue.
 *
 *                                              10-18-93
 *
 **********************************************************/
static Int2 GetSPSitesMod(std::string& retstr)
{
    Int2 ret = ParFlatSPSitesModB;

    for(Int2 i = ParFlatSPSitesModB; i <= ParFlatSPSitesModE; i++)
    {
        size_t pos = NStr::FindNoCase(retstr, ParFlat_SPFeat[i].keystring, 0);
        if (pos == NPOS)
            continue;

        size_t len = StringLen(ParFlat_SPFeat[i].keystring);
        if ((pos != 0 && retstr[pos - 1] != ' ' && retstr[pos - 1] != '.') ||
            (retstr[pos + len] != '\0' && retstr[pos + len] != ' ' &&
             retstr[pos + len] != '.' && retstr[pos + len] != ';'))
            continue;

        ret = i;
        break;
    }

    return(ret);
}

/**********************************************************
 *
 *   static Int2 SpFeatKeyNameValid(keystr):
 *
 *                                              10-18-93
 *
 **********************************************************/
static Int2 SpFeatKeyNameValid(const Char* keystr)
{
    Int2 i;

    for(i = 0; ParFlat_SPFeat[i].inkey != NULL; i++)
        if(StringICmp(ParFlat_SPFeat[i].inkey, keystr) == 0)
            break;

    if(ParFlat_SPFeat[i].inkey == NULL)
        return(-1);
    return(i);
}

/**********************************************************
 *
 *   static void SPFeatGeneral(pp, spfip, initmet):
 *
 *                                              10-18-93
 *
 **********************************************************/
static void SPFeatGeneral(ParserPtr pp, SPFeatInputPtr spfip,
                          bool initmet, objects::CSeq_annot::C_Data::TFtable& feats)
{
    SPFeatInputPtr temp;

    Int2           indx;
    bool           signal;
    bool           bond;
/* bsv : 03/04/2020 : no Seq-feat.exp-ev setting anymore
    bool           noexp;
*/
    Uint1          type;

    for(temp = spfip; temp != NULL; temp = temp->next)
    {
        FtaInstallPrefix(PREFIX_FEATURE, temp->key.c_str(), temp->from.c_str());

        if (NStr::EqualNocase("VARSPLIC", temp->key))
        {
            ErrPostStr(SEV_WARNING, ERR_FEATURE_ObsoleteFeature,
                       "Obsolete UniProt feature \"VARSPLIC\" found. Replaced with \"VAR_SEQ\".");
            temp->key = "VAR_SEQ";
        }

        if (NStr::EqualNocase(temp->key, "NON_STD"))
        {
            if (NStr::EqualNocase(temp->descrip, "Selenocysteine."))
            {
                temp->key = "SE_CYS";
                temp->descrip.clear();
            }
            else
                temp->key = "MOD_RES";
        }

        indx = SpFeatKeyNameValid(temp->key.c_str());
        if(indx == -1)
        {
            ErrPostStr(SEV_WARNING, ERR_FEATURE_UnknownFeatKey, "dropping");
            FtaDeletePrefix(PREFIX_FEATURE);
            continue;
        }

        signal = false;
        bond = false;

/* bsv : 03/04/2020 : no Seq-feat.exp-ev setting anymore
        noexp = SPFeatNoExp(pp, temp);
*/

        CRef<objects::CSeq_feat> feat(new objects::CSeq_feat);

        type = ParFlat_SPFeat[indx].type;
        if(type == ParFlatSPSites)
        {
            if(indx == ParFlatSPSitesModB)
                indx = GetSPSitesMod(temp->descrip);

            feat->SetData().SetSite(static_cast<objects::CSeqFeatData::ESite>(ParFlat_SPFeat[indx].keyint));
        }
        else if(type == ParFlatSPBonds)
        {
            feat->SetData().SetBond(static_cast<objects::CSeqFeatData::EBond>(ParFlat_SPFeat[indx].keyint));
            bond = true;
        }
        else if(type == ParFlatSPRegions)
        {
            feat->SetData().SetRegion(ParFlat_SPFeat[indx].keystring);
            if (feat->GetData().GetRegion() == "Signal")
                signal = true;
        }
        else if(type == ParFlatSPImports)
        {
            feat->SetData().SetImp().SetKey(ParFlat_SPFeat[indx].keystring);
            feat->SetData().SetImp().SetDescr("uncertain amino acids");
        }
        else
        {
            if(type != ParFlatSPInitMet && type != ParFlatSPNonTer &&
               type != ParFlatSPNonCons)
            {
                ErrPostEx(SEV_WARNING, ERR_FEATURE_Dropped,
                          "Swiss-Prot feature \"%s\" with unknown type dropped.",
                          temp->key.c_str());
            }
            FtaDeletePrefix(PREFIX_FEATURE);
            continue;
        }

/* bsv : 03/04/2020 : no Seq-feat.exp-ev setting anymore
        if(noexp)
            feat->SetExp_ev(objects::CSeq_feat::eExp_ev_not_experimental);
        else
            feat->SetExp_ev(objects::CSeq_feat::eExp_ev_experimental);
*/


        CRef<objects::CSeq_loc> loc = GetSPSeqLoc(pp, temp, bond, initmet, signal);
        if (loc.NotEmpty())
            feat->SetLocation(*loc);

        if(SeqLocHaveFuzz(*loc))
            feat->SetPartial(true);

        if (!temp->descrip.empty())
            feat->SetComment(NStr::Sanitize(temp->descrip));

        feats.push_back(feat);

        FtaDeletePrefix(PREFIX_FEATURE);
    }
}

/**********************************************************/
static void DelParenthesis(char* str)
{
    char* p;
    char* q;
    char* pp;
    char* qq;
    char* r;
    Int2    count;
    Int2    left;
    Int2    right;

    for(p = str; *p == ' ' || *p == '\t';)
        p++;
    for(q = p; *q != '\0';)
        q++;
    if(q > p)
        for(q--; (*q == ' ' || *q == '\t') && q > p;)
            *q-- = '\0';
    if(q == p && (*q == ' ' || *q == '\t'))
        *q = '\0';
    for(pp = p; *pp == '(';)
        pp++;
    for(qq = q; *qq == ')' && qq >= pp;)
        qq--;
    for(count = 0, left = 0, right = 0, r = pp; r <= qq; r++)
    {
        if(*r == '(')
            left++;
        else if(*r == ')')
        {
            right++;
            count = left - right;
        }
    }
    if(count < 0)
        for(; count < 0 && pp > p; pp--)
            count++;
    for(count = 0, r = qq; r >= pp; r--)
    {
        if(*r == '(')
            count--;
        else if(*r == ')')
            count++;
    }
    if(count < 0)
        for(; count < 0 && qq < q; qq++)
            count++;
    *++qq = '\0';
    if(pp != str)
        fta_StringCpy(str, pp);
}

/**********************************************************
 *
 *   static void CkGeneNameSP(gname):
 *
 *      Legal characters for gene_name are 0-9, a-z, A-Z,
 *   under-score, dash, period, single quote, back single
 *   quote, slash.
 *
 *                                              10-25-93
 *
 **********************************************************/
static void CkGeneNameSP(char* gname)
{
    char* p;

    DelParenthesis(gname);
    for(p = gname; *p != '\0'; p++)
        if(!(IS_ALPHANUM(*p) || *p == '_' || *p == '-' || *p == '.' ||
           *p == '\'' || *p == '`' || *p == '/' || *p == '(' || *p == ')'))
            break;
    if(*p != '\0')
        ErrPostEx(SEV_WARNING, ERR_GENENAME_IllegalGeneName,
                  "gene_name contains unusual characters, %s, in SWISS-PROT",
                  gname);
}

/**********************************************************
 *
 *   static void ParseGeneNameSP(str, feat):
 *
 *      gene_name and synonyms separated by " OR ".
 *
 *                                              10-25-93
 *
 **********************************************************/
static void ParseGeneNameSP(char* str, objects::CSeq_feat& feat)
{
    char*    gname;
    char*    p;
    char*    q;
    char*    r;

    Int2       count;

    count = 0;
    gname = NULL;

    objects::CGene_ref& gene = feat.SetData().SetGene();

    for(p = str; *p != '\0';)
    {
        while(*p == ' ')
            p++;
        for(q = p; *p != '\0' && *p != ' ';)
            p++;
        if(*p != '\0')
            *p++ = '\0';
        if(StringCmp(q, "AND") == 0 || StringCmp(q, "OR") == 0)
            continue;
        r = StringSave(q);
        CkGeneNameSP(r);
        if(count == 0)
        {
            count++;
            gname = r;
        }
        else
        {
            gene.SetSyn().push_back(r);
            MemFree(r);
        }
    }

    gene.SetLocus(gname);
    MemFree(gname);
}

/**********************************************************
*
*   static CRef<objects::CSeq_loc> GetSeqLocIntSP(seqlen, acnum,
                          *                                   accver, vernum):
*
*                                              10-18-93
*
**********************************************************/
static CRef<objects::CSeq_loc> GetSeqLocIntSP(size_t seqlen, char* acnum, bool accver,
                                                          Int2 vernum)
{
    CRef<objects::CSeq_loc> loc(new objects::CSeq_loc);
    objects::CSeq_interval& interval = loc->SetInt();

    interval.SetFrom(0);
    interval.SetTo(static_cast<TSeqPos>(seqlen) - 1);
    interval.SetId(*MakeAccSeqId(acnum, objects::CSeq_id::e_Swissprot, accver, vernum, false, false));

    return loc;
}

/**********************************************************
 *
 *   static void GetOneGeneRef(pp, hsfp, bptr,
 *                                   seqlen):
 *
 *      Each Gene-ref separated by " AND ".
 *
 *                                              10-25-93
 *
 **********************************************************/
static void GetOneGeneRef(ParserPtr pp, objects::CSeq_annot::C_Data::TFtable& feats, char* bptr,
                          size_t seqlen)
{
    IndexblkPtr ibp;

    char*     str;
    char*     ptr;

    if(pp == NULL || pp->entrylist == NULL)
        return;

    ibp = pp->entrylist[pp->curindx];
    if(ibp == NULL)
        return;

    str = StringSave(bptr);
    for(ptr = str; *ptr != '\0'; ptr++)
        if(*ptr == '\t')
            *ptr = ' ';

    CleanTailNoneAlphaChar(str);

    CRef<objects::CSeq_feat> feat(new objects::CSeq_feat);
    ParseGeneNameSP(str, *feat);
    feat->SetLocation(*GetSeqLocIntSP(seqlen, ibp->acnum, pp->accver, ibp->vernum));

    feats.push_back(feat);
}

/**********************************************************/
static void SPFreeGenRefTokens(char* name, char* syns,
                               char* ltags, char* orfs)
{
    if(name != NULL)
        MemFree(name);
    if(syns != NULL)
        MemFree(syns);
    if(ltags != NULL)
        MemFree(ltags);
    if(orfs != NULL)
        MemFree(orfs);
}

/**********************************************************/
static void SPParseGeneRefTag(char* str, objects::CGene_ref& gene, bool set_locus_tag)
{
    char* p;
    char* q;

    if(str == NULL)
        return;

    for(p = str; p != NULL && *p != '\0'; p = q)
    {
        while(*p == ' ' || *p == ',')
            p++;
        q = StringChr(p, ',');
        if(q != NULL)
            *q++ = '\0';
        if(q == p)
            continue;
        if (set_locus_tag && !gene.IsSetLocus_tag())
        {
            gene.SetLocus_tag(p);
            continue;
        }

        gene.SetSyn().push_back(p);
    }
}

/**********************************************************/
static void SPGetOneGeneRefNew(ParserPtr pp, objects::CSeq_annot::C_Data::TFtable& feats,
                               size_t seqlen, char* name, char* syns,
                               char* ltags, char* orfs)
{
    IndexblkPtr ibp;

    if(pp == NULL || pp->entrylist == NULL ||
       (name == NULL && syns == NULL && ltags == NULL && orfs == NULL))
        return;

    ibp = pp->entrylist[pp->curindx];
    if(ibp == NULL)
        return;

    CRef<objects::CSeq_feat> feat(new objects::CSeq_feat);
    objects::CGene_ref& gene = feat->SetData().SetGene();

    if(name != NULL)
        gene.SetLocus(name);


    SPParseGeneRefTag(syns, gene, false);
    SPParseGeneRefTag(ltags, gene, true);
    SPParseGeneRefTag(orfs, gene, true);

    feat->SetLocation(*GetSeqLocIntSP(seqlen, ibp->acnum, pp->accver, ibp->vernum));

    feats.push_back(feat);
}

/**********************************************************/
static void SPGetGeneRefsNew(ParserPtr pp, objects::CSeq_annot::C_Data::TFtable& feats,
                                   char* bptr, size_t seqlen)
{
    IndexblkPtr ibp;

    char*     name;
    char*     syns;
    char*     ltags;
    char*     orfs;
    char*     str;
    char*     p;
    char*     q;
    char*     r;

    if(pp == NULL || pp->entrylist == NULL || bptr == NULL)
        return;

    ibp = pp->entrylist[pp->curindx];
    if(ibp == NULL)
        return;

    str = StringSave(bptr);

    name = NULL;
    syns = NULL;
    ltags = NULL;
    orfs = NULL;
    for(p = str; p != NULL && *p != '\0'; p = q)
    {
        while(*p == ' ' || *p == ';')
            p++;
        for(r = p;; r = q + 1)
        {
            q = StringChr(r, ';');
            if(q == NULL || q[1] == ' ' || q[1] == '\n' || q[1] == '\0')
                break;
        }
        if(q != NULL)
            *q++ = '\0';
        if(StringNICmp(p, "Name=", 5) == 0)
        {
            if(name != NULL)
            {
                ErrPostEx(SEV_REJECT, ERR_FORMAT_ExcessGeneFields,
                          "Field \"Name=\" occurs multiple times within a GN line. Entry dropped.");
                ibp->drop = 1;
                break;
            }
            p += 5;
            if(p != q)
                name = StringSave(p);
        }
        else if(StringNICmp(p, "Synonyms=", 9) == 0)
        {
            if(syns != NULL)
            {
                ErrPostEx(SEV_REJECT, ERR_FORMAT_ExcessGeneFields,
                          "Field \"Synonyms=\" occurs multiple times within a GN line. Entry dropped.");
                ibp->drop = 1;
                break;
            }
            p += 9;
            if(p != q)
                syns = StringSave(p);
        }
        else if(StringNICmp(p, "OrderedLocusNames=", 18) == 0)
        {
            if(ltags != NULL)
            {
                ErrPostEx(SEV_REJECT, ERR_FORMAT_ExcessGeneFields,
                          "Field \"OrderedLocusNames=\" occurs multiple times within a GN line. Entry dropped.");
                ibp->drop = 1;
                break;
            }
            p += 18;
            if(p != q)
                ltags = StringSave(p);
        }
        else if(StringNICmp(p, "ORFNames=", 9) == 0)
        {
            if(orfs != NULL)
            {
                ErrPostEx(SEV_REJECT, ERR_FORMAT_ExcessGeneFields,
                          "Field \"ORFNames=\" occurs multiple times within a GN line. Entry dropped.");
                ibp->drop = 1;
                break;
            }
            p += 9;
            if(p != q)
                orfs = StringSave(p);
        }
        else if(StringNICmp(p, "and ", 4) == 0)
        {
            if(q != NULL)
                *--q = ';';
            q = p + 4;

            if(name == NULL && syns == NULL && ltags == NULL && orfs == NULL)
                continue;

            if(name == NULL && syns != NULL)
            {
                ErrPostEx(SEV_ERROR, ERR_FORMAT_MissingGeneName,
                          "Encountered a gene with synonyms \"%s\" that lacks a gene symbol.",
                          syns);
            }

            SPGetOneGeneRefNew(pp, feats, seqlen, name, syns,
                               ltags, orfs);
            SPFreeGenRefTokens(name, syns, ltags, orfs);
            name = NULL;
            syns = NULL;
            ltags = NULL;
            orfs = NULL;
        }
        else
        {
            ErrPostEx(SEV_REJECT, ERR_FORMAT_UnknownGeneField,
                      "Field \"%s\" is not a legal field for the GN linetype. Entry dropped.",
                      p);
            ibp->drop = 1;
            break;
        }
    }

    MemFree(str);

    if(name == NULL && syns == NULL && ltags == NULL && orfs == NULL)
        return;

    if(ibp->drop == 1)
    {
        SPFreeGenRefTokens(name, syns, ltags, orfs);
        return;
    }

    SPGetOneGeneRefNew(pp, feats, seqlen, name, syns, ltags, orfs);

    SPFreeGenRefTokens(name, syns, ltags, orfs);
}

/**********************************************************
 *
 *   static Int4 GetSeqLen(entry):
 *
 *                                              11-3-93
 *
 **********************************************************/
static Int4 GetSeqLen(DataBlkPtr entry)
{
    EntryBlkPtr ebp = (EntryBlkPtr) entry->data;
    const objects::CBioseq& bioseq = ebp->seq_entry->GetSeq();
    return bioseq.GetLength();
}

/**********************************************************
 *
 *   static void SPFeatGeneRef(pp, hsfp, entry):
 *
 *      sfp->data: gene (Gene-ref).
 *      Data from GN lines:
 *      - legal characters for gene_name are 0-9, a-z, A-Z,
 *        under-score, dash, period, single quote, back
 *        single quote, slash;
 *      - each Gene-ref separated by " AND ";
 *      - gene_name and synonyms separated by " OR ", the
 *        first one before " OR " is gene_name, others are
 *        synonyms.
 *
 *      sfp->location: SEQLOC_INT, always from 0 to
 *   seqence_length.
 *
 *      Output warning message:
 *      - if DE line containing "(GENE NAME:...)" clause
 *        (SPFeatProtRef routine);
 *      - or other illegal character s.t. white space in the
 *        gene_name.
 *
 *                                              10-25-93
 *
 **********************************************************/
static void SPFeatGeneRef(ParserPtr pp, objects::CSeq_annot::C_Data::TFtable& feats, DataBlkPtr entry)
{
    char* offset;
    char* str;

    size_t len = 0;
    offset = SrchNodeType(entry, ParFlatSP_GN, &len);
    if(offset == NULL)
        return;

    str = GetBlkDataReplaceNewLine(offset, offset + len, ParFlat_COL_DATA_SP);
    StripECO(str);
    if(str == NULL)
        return;

    len = GetSeqLen(entry);
    if(StringIStr(str, "Name=") == NULL &&
       StringIStr(str, "Synonyms=") == NULL &&
       StringIStr(str, "OrderedLocusNames=") == NULL &&
       StringIStr(str, "ORFNames=") == NULL)
        GetOneGeneRef(pp, feats, str, len);
    else
        SPGetGeneRefsNew(pp, feats, str, len);

    MemFree(str);
}

/**********************************************************/
static void SPValidateEcnum(std::string& ecnum)
{
    char* p;
    char* q;
    char* buf;
    Int4    count;

    buf = StringSave(ecnum.c_str());
    for(count = 0, q = buf;; q = p)
    {
        p = q;
        count++;
        if(*p == '-')
        {
            p++;
            if(*p != '.')
                break;
            p++;
            continue;
        }
        if(*p == 'n')
        {
            p++;
            if(*p == '.' || *p == '\0')
            {
                count = 0;
                break;
            }
        }
        while(*p >= '0' && *p <= '9')
            p++;
        if(*q == 'n' && (*p == '.' || *p == '\0'))
        {
            fta_StringCpy(q + 1, p);
            p = q + 1;
        }
        if(p == q)
        {
            count = 0;
            break;
        }
        if(*p != '.')
            break;
        p++;
    }

    if(count != 4 || *p != '\0')
    {
        ErrPostEx(SEV_ERROR, ERR_FORMAT_InvalidECNumber,
                  "Invalid EC number provided in SwissProt DE line: \"%s\". Preserve it anyway.",
                  ecnum.c_str());
    }
    else
        ecnum = buf;
    MemFree(buf);
}

/**********************************************************/
static void SPCollectProtNames(SPDEFieldsPtr sfp, objects::CProt_ref& prot,
                               Int4 tag)
{
    Char ch;

    for(; sfp != NULL; sfp = sfp->next)
    {
        if(sfp->tag == SPDE_RECNAME || sfp->tag == SPDE_ALTNAME ||
           sfp->tag == SPDE_SUBNAME || sfp->tag == SPDE_FLAGS)
            break;
        if(sfp->tag != tag)
            continue;

        ch = *sfp->end;
        *sfp->end = '\0';

        prot.SetName().push_back(sfp->start);
        *sfp->end = ch;
    }
}

/**********************************************************/
static void SPValidateDefinition(SPDEFieldsPtr sfp, Uint1* drop, bool is_trembl)
{
    SPDEFieldsPtr tsfp;
    Int4          rcount;
    Int4          scount;
    Int4          fcount;

    for(rcount = 0, scount = 0, tsfp = sfp; tsfp != NULL; tsfp = tsfp->next)
    {
        if(tsfp->tag == SPDE_RECNAME)
            rcount++;
        else if(tsfp->tag == SPDE_SUBNAME)
            scount++;
    }

    for(fcount = 0, tsfp = sfp; tsfp != NULL; tsfp = tsfp->next)
    {
        if(tsfp->tag != SPDE_RECNAME)
            continue;
        for(tsfp = tsfp->next; tsfp != NULL; tsfp = tsfp->next)
        {
            if(tsfp->tag == SPDE_RECNAME || tsfp->tag == SPDE_ALTNAME ||
               tsfp->tag == SPDE_SUBNAME || tsfp->tag == SPDE_FLAGS)
                break;
            if(tsfp->tag == SPDE_FULL)
                fcount++;
        }
        if(tsfp == NULL)
            break;
    }

    if(rcount > 1)
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_MultipleRecName,
                  "This UniProt record has multiple RecName protein-name categories, but only one is allowed. Entry dropped.");
        *drop = 1;
    }
    else if(rcount == 0 && !is_trembl)
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_MissingRecName,
                  "This UniProt/Swiss-Prot record lacks required RecName protein-name categorie. Entry dropped.");
        *drop = 1;
    }

    if(scount > 0 && !is_trembl)
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_SwissProtHasSubName,
                  "This UniProt/Swiss-Prot record includes a SubName protein-name category, which should be used only for UniProt/TrEMBL. Entry dropped.");
        *drop = 1;
    }

    if(fcount == 0 && rcount > 0)
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_MissingFullRecName,
                  "This UniProt record lacks a Full name in the RecName protein-name category.");
        *drop = 1;
    }
}

/**********************************************************/
static void SPParseDefinition(char* str, const objects::CBioseq::TId& ids, IndexblkPtr ibp,
                              objects::CProt_ref& prot)
{
    CharIntLenPtr cilp;
    SPDEFieldsPtr sfp;
    SPDEFieldsPtr tsfp;

    bool          is_trembl;
    char*       p;
    char*       q;
    char*       r;
    Int4          count;
    Char          ch;

    if(str == NULL || (StringNICmp(str, "RecName: ", 9) != 0 &&
       StringNICmp(str, "AltName: ", 9) != 0 &&
       StringNICmp(str, "SubName: ", 9) != 0))
        return;

    is_trembl = false;

    ITERATE(objects::CBioseq::TId, id, ids)
    {
        if (!(*id)->IsSwissprot())
            continue;

        if ((*id)->GetSwissprot().IsSetRelease() &&
            StringICmp((*id)->GetSwissprot().GetRelease().c_str(), "unreviewed") == 0)
            is_trembl = true;
    }

    sfp = (SPDEFieldsPtr) MemNew(sizeof(SPDEFields));
    sfp->tag = 0;
    sfp->next = NULL;

    for(tsfp = sfp, p = str, count = 0; *p != '\0';)
    {
        while(*p == ' ')
            p++;
        for(q = p; *p != '\0' && *p != ' ';)
            p++;
        ch = *p;
        *p = '\0';
        for(cilp = spde_tags; cilp->str != NULL; cilp++)
            if(StringNICmp(cilp->str, q, cilp->len) == 0)
                break;

        *p = ch;
        if(cilp->str == NULL)
            continue;

        if(tsfp->tag != 0)
        {
            if(q == tsfp->start)
                tsfp->end = q;
            else
            {
                for(r = q - 1; *r == ' ' || *r == ';';)
                    r--;
                tsfp->end = r + 1;
            }
        }

        if(cilp->num == SPDE_INCLUDES || cilp->num == SPDE_CONTAINS)
            break;

        count++;
        tsfp->next = (SPDEFieldsPtr) MemNew(sizeof(SPDEFields));
        tsfp = tsfp->next;
        tsfp->tag = cilp->num;
        for(r = q + cilp->len; *r == ' ';)
            r++;
        tsfp->start = r;
        tsfp->next = NULL;
    }

    if(*p == '\0')
        tsfp->end = p;

    SPValidateDefinition(sfp->next, &ibp->drop, is_trembl);

    for(tsfp = sfp->next; tsfp != NULL; tsfp = tsfp->next)
        if(tsfp->tag == SPDE_RECNAME)
            SPCollectProtNames(tsfp->next, prot, SPDE_FULL);
    for(tsfp = sfp->next; tsfp != NULL; tsfp = tsfp->next)
        if(tsfp->tag == SPDE_RECNAME)
            SPCollectProtNames(tsfp->next, prot, SPDE_SHORT);

    for(tsfp = sfp->next; tsfp != NULL; tsfp = tsfp->next)
        if(tsfp->tag == SPDE_ALTNAME)
            SPCollectProtNames(tsfp->next, prot, SPDE_FULL);
    for(tsfp = sfp->next; tsfp != NULL; tsfp = tsfp->next)
        if(tsfp->tag == SPDE_ALTNAME)
            SPCollectProtNames(tsfp->next, prot, SPDE_SHORT);

    for(tsfp = sfp->next; tsfp != NULL; tsfp = tsfp->next)
        if(tsfp->tag == SPDE_SUBNAME)
            SPCollectProtNames(tsfp->next, prot, SPDE_FULL);
    for(tsfp = sfp->next; tsfp != NULL; tsfp = tsfp->next)
        if(tsfp->tag == SPDE_SUBNAME)
            SPCollectProtNames(tsfp->next, prot, SPDE_SHORT);

}

/**********************************************************/
static void SPGetPEValue(DataBlkPtr entry, objects::CSeq_feat& feat)
{
    char*   offset;
    char*   buf;
    char*   p;
    char*   q;
    Char      ch;

    size_t len = 0;
    offset = SrchNodeType(entry, ParFlatSP_PE, &len);
    if(offset == NULL || len < 1)
        return;

    ch = offset[len-1];
    offset[len-1] = '\0';
    buf = StringSave(offset);
    offset[len-1] = ch;

    for(q = buf + 2; *q == ' ';)
        q++;
    p = StringChr(q, ':');
    if(p != NULL)
        for(p++; *p == ' ';)
            p++;
    else
        p = q;

    q = StringRChr(p, ';');
    if(q == NULL)
        q = StringChr(p, '\n');
    if(q != NULL)
        *q = '\0';

    if(MatchArrayIString(PE_values, p) < 0)
        ErrPostEx(SEV_ERROR, ERR_SPROT_PELine,
                  "Unrecognized value is encountered in PE (Protein Existence) line: \"%s\".",
                  p);

    CRef<objects::CGb_qual> qual(new objects::CGb_qual);
    qual->SetQual("UniProtKB_evidence");
    qual->SetVal(p);
    feat.SetQual().push_back(qual);

    MemFree(buf);
}

/**********************************************************
 *
 *   static SeqFeatPtr SPFeatProtRef(pp, hsfp, entry,
 *                                   spfbp):
 *
 *      sfp->data: prot (Prot-ref):
 *      - name: DE line, delete everything after " (" or "/";
 *      - EC_number: if DE lines contains "(EC ...)".
 *
 *      sfp->location: SEQLOC_INT, always from 0 to
 *   seqence_length.
 *
 *                                              10-20-93
 *
 **********************************************************/
static void SPFeatProtRef(ParserPtr pp, objects::CSeq_annot::C_Data::TFtable& feats,
                          DataBlkPtr entry, SPFeatBlnPtr spfbp)
{
    IndexblkPtr ibp;

    char*     offset;

    char* str;
    std::string str1;

    char*     ptr;
    char*     bptr;
    char*     eptr;
    char*     s;

    const char  *tag;
    Char        symb;
    Int4        shift;

    EntryBlkPtr ebp;

    ebp = (EntryBlkPtr) entry->data;

    objects::CSeq_entry& seq_entry = *ebp->seq_entry;
    objects::CBioseq& bioseq = seq_entry.SetSeq();

    size_t len = 0;
    offset = SrchNodeType(entry, ParFlatSP_DE, &len);
    if(offset == NULL)
        return;

    CRef<objects::CSeq_feat> feat(new objects::CSeq_feat);
    objects::CProt_ref& prot = feat->SetData().SetProt();

    bptr = offset;
    eptr = bptr + len;

    str = GetBlkDataReplaceNewLine(bptr, eptr, ParFlat_COL_DATA_SP);
    StripECO(str);
    s = str + StringLen(str) - 1;
    while(s >= str && (*s == '.' || *s == ';' || *s == ',' ))
        *s-- = '\0';

    ShrinkSpaces(str);

    ibp = pp->entrylist[pp->curindx];

    if(StringNICmp(str, "Contains: ", 10) == 0 ||
       StringNICmp(str, "Includes: ", 10) == 0)
    {
        ErrPostEx(SEV_REJECT, ERR_FORMAT_NoProteinNameCategory,
                  "DE lines do not have a non-Includes/non-Contains RecName, AltName or SubName protein name category. Entry dropped.");
        ibp->drop = 1;
    }

    if(StringNICmp(str, "RecName: ", 9) == 0 ||
       StringNICmp(str, "AltName: ", 9) == 0 ||
       StringNICmp(str, "SubName: ", 9) == 0)
    {
        tag = "; EC=";
        symb = ';';
        shift = 5;
        SPParseDefinition(str, bioseq.GetId(), ibp, prot);
    }
    else
    {
        tag = "(EC";
        symb = ')';
        shift = 3;
    }

    while((ptr = StringStr(str, tag)) != NULL)
    {
        len = StringLen(str);
        str1.assign(str, ptr);

        ptr += shift;
        while(*ptr == ' ')
            ptr++;

        for(bptr = ptr; *ptr != '\0' && *ptr != ' ' && *ptr != symb;)
            ptr++;
        if(ptr > bptr)
        {
            std::string ecnum(bptr, ptr);
            SPValidateEcnum(ecnum);

            if (!ecnum.empty())
                prot.SetEc().push_back(ecnum);
        }
        else
        {
            ErrPostEx(SEV_WARNING, ERR_FORMAT_ECNumberNotPresent,
                      "Empty EC number provided in SwissProt DE line.");
        }

        if(symb == ')')
        {
            while(*ptr != '\0' && (*ptr == ' ' || *ptr == symb))
                ptr++;
            if(StringLen(ptr) <= 1)
                NStr::TruncateSpacesInPlace(str1, NStr::eTrunc_End);
        }

        str1 += ptr;

        MemFree(str);
        str = StringSave(str1.c_str());
    }

    if(symb == ')')
    {
        while((ptr = StringStr(str, " (")) != NULL ||
              (ptr = StringStr(str, " /")) != NULL)
        {
            str1.assign(str, ptr);
            NStr::TruncateSpacesInPlace(str1, NStr::eTrunc_End);

            MemFree(str);
            str = StringSave(str1.c_str());
        }
    }

    if (!prot.IsSetName())
        prot.SetName().push_back(str);

    MemFree(str);

    feat->SetLocation(*GetSeqLocIntSP(GetSeqLen(entry), ibp->acnum, pp->accver, ibp->vernum));

    if(spfbp->nonter)
    {
        feat->SetPartial(true);

        if (spfbp->noleft)
            GetIntFuzzPtr(4, 2, 0, feat->SetLocation().SetInt().SetFuzz_from());      /* lim, lt, no-min */
        if (spfbp->noright)
            GetIntFuzzPtr(4, 1, 0, feat->SetLocation().SetInt().SetFuzz_to());        /* lim, gt, no-min */
    }

    SPGetPEValue(entry, *feat);

    feats.push_back(feat);
}

/**********************************************************
 *
 *   static SPSegLocPtr GetSPSegLocInfo(sep, spfip, spfbp):
 *
 *      Return a link list of segment location information,
 *   data from NON_CONS and change the modif of the sep of
 *   the bsp->descr to partial.
 *
 *      If input has NON_CONS: 17..18, 31..32, 65..66, and
 *   total seqlen = 100, then SPSegLocPtr, spslp, will have
 *   4 nodes, each node has
 *   0, 16, 16-0+1=17, 1, 4, XXXX_1, descr of XXXX_1, add no-right
 *   17, 30, 30-17+1=14, 2, 4, XXXX_2, descr of XXXX_2, add no-right, no-left
 *   31, 64, 64-31+1=34, 3, 4, XXXX_3, descr of XXXX_3, add no-right, no-left
 *   65, 99, 99-65+1=35, 4, 4, XXXX_4, descr of XXXX_4, add no-left
 *   where XXXX is locus (ID) name.
 *
 *      Set hspslp->fuzzfrom = TRUE if spfbp->noleft = TRUE.
 *      Set hspslp->fuzzto = TRUE if spfbp->noright = TRUE.
 *
 *                                              11-5-93
 *
 **********************************************************/
static SPSegLocPtr GetSPSegLocInfo(objects::CBioseq& bioseq, SPFeatInputPtr spfip,
                                   SPFeatBlnPtr spfbp)
{
    SPSegLocPtr curspslp = NULL;
    SPSegLocPtr hspslp = NULL;
    SPSegLocPtr spslp;
    char*     p;

    if(spfip == NULL)
        return(NULL);

    /* get location range
     */
    for(; spfip != NULL; spfip = spfip->next)
    {
        if (spfip->key != "NON_CONS")
            continue;

        if(hspslp == NULL)
        {
            spslp = (SPSegLocPtr) MemNew(sizeof(SPSegLoc));
            spslp->from = 0;
            p = (char *) spfip->from.c_str();
            if(*p == '<' || *p == '>' || *p == '?')
                p++;

            spslp->len = atoi(p);
            hspslp = spslp;
            curspslp = spslp;
        }
        else
        {
            p = (char *) spfip->from.c_str();
            if(*p == '<' || *p == '>' || *p == '?')
                p++;
            curspslp->len = atoi(p) - curspslp->from;
        }

        spslp = (SPSegLocPtr) MemNew(sizeof(SPSegLoc));
        p = (char *) spfip->from.c_str();
        if(*p == '<' || *p == '>' || *p == '?')
            p++;
        spslp->from = atoi(p);
        curspslp->next = spslp;
        curspslp = spslp;
    }

    NON_CONST_ITERATE(TSeqdescList, descr, bioseq.SetDescr().Set())
    {
        if (!(*descr)->IsMolinfo())
            continue;

        if (spfbp->noleft && spfbp->noright)
            (*descr)->SetMolinfo().SetCompleteness(objects::CMolInfo::eCompleteness_no_ends);
        else if (spfbp->noleft)
            (*descr)->SetMolinfo().SetCompleteness(objects::CMolInfo::eCompleteness_no_left);
        else if (spfbp->noright)
            (*descr)->SetMolinfo().SetCompleteness(objects::CMolInfo::eCompleteness_no_right);
    }

    if(hspslp != NULL)
        curspslp->len = bioseq.GetLength() - curspslp->from;

    return(hspslp);
}

/**********************************************************
 *
 *   static void CkInitMetSP(pp, spfip, sep, spfbp):
 *
 *                                              11-1-93
 *
 **********************************************************/
static void CkInitMetSP(ParserPtr pp, SPFeatInputPtr spfip,
                        objects::CSeq_entry& seq_entry, SPFeatBlnPtr spfbp)
{
    SPFeatInputPtr temp;
    char*        p;
    Int2           count;
    Int4           from = 0;
    Int4           to;

    for(count = 0; spfip != NULL; spfip = spfip->next)
    {
        if (spfip->key != "INIT_MET")
            continue;

        if(count > 0)
            break;

        count++;
        p = (char *) spfip->from.c_str();
        if(*p == '<' || *p == '>' || *p == '?')
            p++;
        from = atoi(p);
        p = (char *) spfip->to.c_str();
        if(*p == '<' || *p == '>' || *p == '?')
            p++;
        to = atoi(p);

        if((from != 0 || to != 0) && (from != 1 || to != 1))
            break;
        temp = spfip;
    }

    if(count == 0)
        return;

    if(spfip != NULL)
    {
        ErrPostEx(SEV_ERROR, ERR_FEATURE_Invalid_INIT_MET,
                  "Either incorrect or more than one INIT_MET feature provided.");
        return;
    }

    if(temp->descrip != NULL)
    {
        ErrPostEx(SEV_WARNING, ERR_FEATURE_ExpectEmptyComment,
                  "%s:%d-%d has description: %s",
                  temp->key.c_str(), from, to, temp->descrip.c_str());
    }


    objects::CBioseq& bioseq = seq_entry.SetSeq();

    objects::CSeq_data& data = bioseq.SetInst().SetSeq_data();
    std::string& sequence = data.SetIupacaa().Set();

    if(from == 0)
    {
        spfbp->initmet = true;

        /* insert "M" in the front
         */
        sequence.insert(sequence.begin(), 'M');
        bioseq.SetInst().SetLength(static_cast<TSeqPos>(sequence.size()));
    }
    else if (sequence.empty() || sequence[0] != 'M')
        ErrPostEx(SEV_ERROR, ERR_FEATURE_MissingInitMet,
                  "The required Init Met is missing from the sequence.");
}

/**********************************************************
 *
 *   static void CkNonTerSP(pp, spfip, sep, spfbp):
 *
 *      Set spfbp->nonter = spfbp->noleft = TRUE if
 *   NON_TER 1..1.
 *      Set spfbp->nonter = spfbp->noright = TRUE if
 *   NON_TER base..base.
 *      Set bsp->descr of modif = partial if there is more
 *   than 5 contiguous unsequenced residues, X.
 *
 *                                              11-2-93
 *
 **********************************************************/
static void CkNonTerSP(ParserPtr pp, SPFeatInputPtr spfip,
                       objects::CSeq_entry& seq_entry, SPFeatBlnPtr spfbp)
{
    SPFeatInputPtr temp;
    Int4           from;
    Int4           ctr;
    bool           segm;

    objects::CMolInfo* mol_info = nullptr;
    objects::CBioseq& bioseq = seq_entry.SetSeq();

    ctr = 0;
    NON_CONST_ITERATE(TSeqdescList, descr, bioseq.SetDescr().Set())
    {
        if (!(*descr)->IsMolinfo())
            continue;

        mol_info = &((*descr)->SetMolinfo());
        break;
    }

    segm = false;
    for(temp = spfip; temp != NULL; temp = temp->next)
    {
        if (temp->key == "NON_CONS")
        {
            segm = true;
            continue;
        }

        if(temp->key != "NON_TER")
            continue;

        from = NStr::StringToInt(temp->from);
        if (from != NStr::StringToInt(temp->to))
        {
            ErrPostStr(SEV_WARNING, ERR_FEATURE_UnEqualEndPoint,
                       "NON_TER has unequal endpoints");
            continue;
        }

        if(from == 1)
        {
            spfbp->nonter = true;
            spfbp->noleft = true;
        }
        else if(from == (Int4) pp->entrylist[pp->curindx]->bases)
        {
            spfbp->nonter = true;
            spfbp->noright = true;
        }
        else
        {
            ErrPostStr(SEV_WARNING, ERR_FEATURE_NotSeqEndPoint,
                       "NON_TER is not at a sequence endpoint.");
        }
    }

    if (mol_info == nullptr)
        return;

    if (segm && mol_info->GetCompleteness() != 2)
    {
        mol_info->SetCompleteness(objects::CMolInfo::eCompleteness_partial);
        ErrPostEx(SEV_WARNING, ERR_FEATURE_NoFragment,
                  "Found NON_CONS in FT line but no FRAGMENT in DE line.");
    }
    else if (spfbp->nonter && mol_info->GetCompleteness() != objects::CMolInfo::eCompleteness_partial)
    {
        mol_info->SetCompleteness(objects::CMolInfo::eCompleteness_partial);
        ErrPostEx(SEV_WARNING, ERR_FEATURE_NoFragment,
                  "Found NON_TER in FT line but no FRAGMENT in DE line.");
    }
    else if (!spfbp->nonter && mol_info->GetCompleteness() == objects::CMolInfo::eCompleteness_partial && !segm)
    {
        ErrPostEx(SEV_WARNING, ERR_FEATURE_PartialNoNonTerNonCons,
                  "Entry is partial but has no NON_TER or NON_CONS features.");
    }
    else if (mol_info->GetCompleteness() != 2)
    {
        if (bioseq.GetInst().IsSetSeq_data())
        {
            const objects::CSeq_data& data = bioseq.GetInst().GetSeq_data();
            const std::string& sequence = data.GetIupacaa().Get();

            for (std::string::const_iterator value = sequence.begin(); value != sequence.end(); ++value) {
                if (*value != 'X') {
                    ctr = 0;                /* reset counter */
                    continue;
                }

                ctr++;
                if (ctr == 5) {
                    mol_info->SetCompleteness(objects::CMolInfo::eCompleteness_partial);
                    break;
                }
            }
        }
    }
}

/**********************************************************/
static void SeqToDeltaSP(objects::CBioseq& bioseq, SPSegLocPtr spslp)
{
    if (spslp == NULL || !bioseq.GetInst().IsSetSeq_data())
        return;

    objects::CSeq_ext::TDelta& deltas = bioseq.SetInst().SetExt().SetDelta();
    const std::string& bioseq_data = bioseq.GetInst().GetSeq_data().GetIupacaa().Get();

    for(;spslp != NULL; spslp = spslp->next)
    {
        CRef<objects::CDelta_seq> delta(new objects::CDelta_seq);
        if (!deltas.Set().empty())
        {
            delta->SetLiteral().SetLength(0);
            delta->SetLiteral().SetFuzz().SetLim();
            deltas.Set().push_back(delta);

            delta.Reset(new objects::CDelta_seq);
        }

        delta->SetLiteral().SetLength(spslp->len);


        std::string data_str = bioseq_data.substr(spslp->from, spslp->len);

        delta->SetLiteral().SetSeq_data().SetIupacaa().Set(data_str);
        deltas.Set().push_back(delta);
    }

    if (deltas.Set().size() > 1)
    {
        bioseq.SetInst().SetRepr(objects::CSeq_inst::eRepr_delta);
        bioseq.SetInst().ResetSeq_data();
    }
    else
        bioseq.SetInst().SetExt().Reset();
}

/**********************************************************
 *
 *   static void GetSPAnnot(pp, entry, protconv):
 *
 *                                              10-15-93
 *
 **********************************************************/
static void GetSPAnnot(ParserPtr pp, DataBlkPtr entry, unsigned char* protconv)
{
    SPFeatInputPtr spfip;
    EntryBlkPtr    ebp;

    SPFeatBlnPtr   spfbp;
    SPSegLocPtr    spslp;       /* segment location, data from NON_CONS */
    SPSegLocPtr    next;

    ebp = (EntryBlkPtr) entry->data;
    objects::CSeq_entry& seq_entry = *ebp->seq_entry;

    spfbp = (SPFeatBlnPtr) MemNew(sizeof(SPFeatBln));
    spfbp->initmet = false;
    spfbp->nonter = false;
    spfbp->noright = false;
    spfbp->noleft = false;

    spfip = ParseSPFeat(entry, pp->entrylist[pp->curindx]->bases);

    objects::CSeq_annot::C_Data::TFtable feats;

    if (spfip != NULL)
    {
        CkNonTerSP(pp, spfip, seq_entry, spfbp);
        CkInitMetSP(pp, spfip, seq_entry, spfbp);
        SPFeatGeneral(pp, spfip, spfbp->initmet, feats);
    }

    SPFeatGeneRef(pp, feats, entry);        /* GN line */
    SPFeatProtRef(pp, feats, entry, spfbp); /* DE line */

    objects::CBioseq& bioseq = seq_entry.SetSeq();

    spslp = GetSPSegLocInfo(bioseq, spfip, spfbp); /* checking NON_CONS key */
    if(spslp != NULL)
        SeqToDeltaSP(bioseq, spslp);

    if (!feats.empty())
    {
        CRef<objects::CSeq_annot> annot(new objects::CSeq_annot);
        annot->SetData().SetFtable().swap(feats);
        bioseq.SetAnnot().push_back(annot);
    }

    for(; spslp != NULL; spslp = next)
    {
        next = spslp->next;
        MemFree(spslp);
    }

    FreeSPFeatInputSet(spfip);
    MemFree(spfbp);
}

/**********************************************************/
static void SpPrepareEntry(ParserPtr pp, DataBlkPtr entry, unsigned char* protconv)
{
    Int2        curkw;
    char*     ptr;
    char*     eptr;
    EntryBlkPtr ebp;

    ebp = (EntryBlkPtr) entry->data;
    ptr = entry->offset;
    eptr = ptr + entry->len;
    for(curkw = ParFlatSP_ID; curkw != ParFlatSP_END;)
    {
        ptr = GetEmblBlock(&ebp->chain, ptr, &curkw, pp->format, eptr);
    }
    GetSprotSubBlock(pp, entry);

    if(pp->entrylist[pp->curindx]->bases == 0)
    {
        SpAddToIndexBlk(entry, pp->entrylist[pp->curindx]);
    }

    CRef<objects::CBioseq> bioseq = CreateEntryBioseq(pp, false);
    ebp->seq_entry.Reset(new objects::CSeq_entry);
    ebp->seq_entry->SetSeq(*bioseq);
    GetScope().AddBioseq(*bioseq);

    GetSprotDescr(*bioseq, pp, entry);

    GetSPInst(pp, entry, protconv);
    GetSPAnnot(pp, entry, protconv);

    TEntryList entries;
    entries.push_back(ebp->seq_entry);
    fta_find_pub_explore(pp, entries);

    if(pp->citat)
    {
        StripSerialNumbers(entries);
    }
}

/**********************************************************
 *
 *   bool SprotAscii(pp):
 *
 *      Return FALSE if allocate entry block failed.
 *
 *                                              3-23-93
 *
 **********************************************************/
bool SprotAscii(ParserPtr pp)
{
    DataBlkPtr  entry;
    unsigned char*    protconv;

    Int4        total;
    Int4        i;
    IndexblkPtr ibp;
    Int4        imax;

    protconv = GetProteinConv();

    for(total = 0, i = 0, imax = pp->indx; i < imax; i++)
    {
        pp->curindx = i;
        ibp = pp->entrylist[i];

        err_install(ibp, pp->accver);

        if(ibp->drop != 1)
        {
            entry = LoadEntry(pp, ibp->offset, ibp->len);
            if(entry == NULL)
            {
                FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);
                return false;
            }

            SpPrepareEntry(pp, entry, protconv);

            if(ibp->drop != 1)
            {
                CRef<objects::CSeq_entry>& cur_entry = ((EntryBlkPtr)entry->data)->seq_entry;
                pp->entries.push_back(cur_entry);

                cur_entry.Reset();
            }
            FreeEntry(entry);
        }
        if(ibp->drop != 1)
        {
            total++;
            ErrPostEx(SEV_INFO, ERR_ENTRY_Parsed,
                      "OK - entry \"%s|%s\" parsed successfully",
                      ibp->locusname, ibp->acnum);
        }
        else
        {
            ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped, "Entry \"%s|%s\" skipped",
                      ibp->locusname, ibp->acnum);
        }
    }

    FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);

    ErrPostEx(SEV_INFO, ERR_ENTRY_ParsingComplete,
              "Parsing completed, %d entr%s parsed",
              total, (total == 1) ? "y" : "ies");
    MemFree(protconv);
    return true;
}

END_NCBI_SCOPE
