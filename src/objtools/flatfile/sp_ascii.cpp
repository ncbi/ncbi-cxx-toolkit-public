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
 * File Name: sp_ascii.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
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
USING_SCOPE(objects);

const char* ParFlat_SPComTopics[] = {
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
    nullptr
};

/* bsv : 03/04/2020 : no Seq-feat.exp-ev setting anymore
const char* ParFlat_SPFeatNoExp[] = {
    "(PROBABLE).",
    "(PROBABLE)",
    "PROBABLE.",
    "(POTENTIAL).",
    "(POTENTIAL)",
    "POTENTIAL.",
    "(BY SIMILARITY).",
    "(BY SIMILARITY)",
    "BY SIMILARITY.",
    nullptr
};

const char* ParFlat_SPFeatNoExpW[] = {
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
    nullptr
};
*/

SPFeatType ParFlat_SPFeat[] = {
    { "ACT_SITE", ParFlatSPSites, 1, nullptr },
    { "BINDING", ParFlatSPSites, 2, nullptr },
    { "CARBOHYD", ParFlatSPSites, 6, nullptr },
    { "MUTAGEN", ParFlatSPSites, 8, nullptr },
    { "METAL", ParFlatSPSites, 9, nullptr },
    { "LIPID", ParFlatSPSites, 20, nullptr },
    { "NP_BIND", ParFlatSPSites, 21, nullptr },
    { "DNA_BIND", ParFlatSPSites, 22, nullptr },
    { "SITE", ParFlatSPSites, 255, nullptr },
    { "MOD_RES", ParFlatSPSites, 5, nullptr }, /* 9 */
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
    { "MOD_RES", ParFlatSPSites, 15, "Sulfoserine" },
    { "MOD_RES", ParFlatSPSites, 15, "Sulfothreonine" },
    { "MOD_RES", ParFlatSPSites, 15, "Sulfotyrosine" },
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
    { "MOD_RES", ParFlatSPSites, 19, "Blocked carboxyl end (His)" }, /* 174 */
    { "DISULFID", ParFlatSPBonds, 1, nullptr },
    { "THIOLEST", ParFlatSPBonds, 2, nullptr },
    { "CROSSLNK", ParFlatSPBonds, 3, nullptr },
    { "THIOETH", ParFlatSPBonds, 4, nullptr },
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
    { nullptr, 0, 0, nullptr }
};

/* for array index, MOD_RES in the "ParFlat_SPFeat"
 */
#define ParFlatSPSitesModB 9   /* beginning */
#define ParFlatSPSitesModE 174 /* end */

#define COPYRIGHT  "This Swiss-Prot entry is copyright."
#define COPYRIGHT1 "Copyrighted by the UniProt Consortium,"

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

struct CharIntLen {
    string_view str;
    Int4        num;
    Int4        len;
};

struct SPDEField {
    Int4        tag;
    const char* start;
    const char* end = nullptr;

    SPDEField(Int4 t, const char* s) :
        tag(t), start(s) {}
};
using SPDEFieldList = forward_list<SPDEField>;

struct SPFeatInput {
    string key;     /* column 6-13 */
    string from;    /* column 15-20 */
    string to;      /* column 22-27 */
    string descrip; /* column 35-75, continue line if a blank key */
};
using SPFeatInputList = forward_list<SPFeatInput>;

struct SPFeatBln {
    bool initmet = false;
    bool nonter  = false;
    bool noright = false;
    bool noleft  = false;
};
using SPFeatBlnPtr = SPFeatBln*;

/* segment location, data from NON_CONS
 */
struct SPSegLoc {
    Int4 from;    /* the beginning point of the segment */
    Int4 len = 0; /* total length of the segment */

    SPSegLoc(Int4 f) :
        from(f) {}
};
using SPSegLocList = forward_list<SPSegLoc>;

struct SetOfSyns {
    char*      synname = nullptr;
    SetOfSyns* next    = nullptr;
};
using SetOfSynsPtr = SetOfSyns*;

struct SetOfSpecies {
    char*      fullname = nullptr;
    char*      name     = nullptr;
    SetOfSyns* syn      = nullptr;
};
using SetOfSpeciesPtr = SetOfSpecies*;

struct ViralHost {
    TTaxId     taxid = ZERO_TAX_ID;
    char*      name  = nullptr;
};
using ViralHostList = forward_list<ViralHost>;

struct EmblAcc {
    CSeq_id::E_Choice choice;
    string            acc;
    string            pid;

    EmblAcc(CSeq_id::E_Choice c, string_view a, string_view p) :
        choice(c), acc(a), pid(p)
    {
    }
};
using TEmblAccList = forward_list<EmblAcc>;

// clang-format off
const CharIntLen spde_tags[] = {
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
    {{},            0,                0},
};

const char* org_mods[] = {
    "STRAIN",    "SUBSTRAIN", "TYPE",     "SUBTYPE",  "VAR.",     "SEROTYPE",
    "SEROGROUP", "SEROVAR",   "CULTIVAR", "PATHOVAR", "CHEMOVAR", "BIOVAR",
    "BIOTYPE",   "GROUP",     "SUBGROUP", "ISOLATE",  "ACRONYM",  "DOSAGE",
    "NAT_HOST",  "SUBSP.",    nullptr
};

const char* obsolete_dbs[] = {
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
    nullptr
};

const char* valid_dbs[] = {
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
    "XENBASE",                "ZFIN",                   nullptr
};

const char* SP_organelle[] = {
    "CHLOROPLAST", "CYANELLE", "MITOCHONDRION", "PLASMID", "NUCLEOMORPH",
    "HYDROGENOSOME", "APICOPLAST", "CHROMATOPHORE",
    "ORGANELLAR CHROMATOPHORE", nullptr
};

const char* PE_values[] = {
    "Evidence at protein level",
    "Evidence at transcript level",
    "Inferred from homology",
    "Predicted",
    "Uncertain",
    nullptr
};
// clang-format on

/**********************************************************
 *
 *   static char* StringCombine(str1, str2, delim):
 *
 *      Return a string which is combined str1 and str2,
 *   put blank between two strings if "blank" = TRUE;
 *   also memory free out str1 and str2.
 *
 **********************************************************/
static void StringCombine(string& dest, const string& to_add, const Char* delim)
{
    if (to_add.empty())
        return;

    if (delim && *delim != '\0' && ! dest.empty())
        dest += delim[0];

    dest += to_add;
}

/**********************************************************
 *
 *   static CRef<CDbtag> MakeStrDbtag(dbname, str):
 *
 *                                              10-1-93
 *
 **********************************************************/
static CRef<CDbtag> MakeStrDbtag(const char* dbname, const char* str)
{
    CRef<CDbtag> tag;

    if (dbname && str) {
        tag.Reset(new CDbtag);
        tag->SetDb(dbname);
        tag->SetTag().SetStr(str);
    }

    return tag;
}

/**********************************************************
 *
 *   static CRef<CDate> MakeDatePtr(str, source):
 *
 *      Return a DatePtr with "std" type if dd-mmm-yyyy
 *   or dd-mmm-yy format; with "str" type if not
 *   a dd-mmm-yyyy format.
 *
 **********************************************************/
static CRef<CDate> MakeDatePtr(const char* str, Parser::ESource source)
{
    static Char msg[11];

    CRef<CDate> res(new CDate);

    if (! str)
        return res;

    if (StringChr(str, '-') && (isdigit(*str) != 0 || *str == ' ')) {
        CRef<CDate_std> std_date = get_full_date(str, true, source);
        res->SetStd(*std_date);
        if (XDateCheck(*std_date) != 0) {
            StringNCpy(msg, str, 10);
            msg[10] = '\0';
            FtaErrPost(SEV_WARNING, ERR_REFERENCE_IllegalDate, "Illegal date: {}", msg);
        }
    }

    if (res->Which() == CDate::e_not_set) {
        res->SetStr(str);
    }

    return res;
}

/**********************************************************/
static void fta_create_pdb_seq_id(CSP_block_Base::TSeqref& refs, const char* mol, Uint1 chain)
{
    if (! mol)
        return;

    CRef<CPDB_seq_id> pdb_seq_id(new CPDB_seq_id);
    pdb_seq_id->SetMol(CPDB_mol_id(mol));

    if (chain > 0) {
        pdb_seq_id->SetChain(chain);
    }

    CRef<CSeq_id> sid(new CSeq_id);
    sid->SetPdb(*pdb_seq_id);
    refs.push_back(sid);
}

/**********************************************************/
static void MakeChainPDBSeqId(CSP_block_Base::TSeqref& refs, const char* mol, char* chain)
{
    char* fourth;
    char* p;
    char* q;
    char* r;

    bool bad;
    bool got;

    if (! mol || ! chain)
        return;

    fourth = StringSave(chain);
    for (bad = false, got = false, q = chain; *q != '\0'; q = p) {
        while (*q == ' ' || *q == ',')
            q++;
        for (p = q; *p != '\0' && *p != ' ' && *p != ',';)
            p++;
        if (*p != '\0')
            *p++ = '\0';
        r = StringRChr(q, '=');
        if (! r && ! got) {
            fta_create_pdb_seq_id(refs, mol, 0);
            continue;
        }
        *r = '\0';
        for (r = q; *r != '\0'; r++) {
            if (*r == '/')
                continue;
            if (r[1] != '/' && r[1] != '\0') {
                while (*r != '/' && *r != '\0')
                    r++;
                r--;
                bad = true;
                continue;
            }
            got = true;
            fta_create_pdb_seq_id(refs, mol, *r);
        }
    }

    if (bad) {
        FtaErrPost(SEV_ERROR, ERR_FORMAT_InvalidPDBCrossRef, "PDB cross-reference \"{}\" contains one or more chain identifiers that are more than a single character in length.", fourth);
        if (! got)
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
static void MakePDBSeqId(CSP_block_Base::TSeqref& refs, const char* mol, const char* rel, char* chain, bool* drop, Parser::ESource source)
{
    if (! mol)
        return;

    if (! chain) {
        CRef<CPDB_seq_id> pdb_seq_id(new CPDB_seq_id);
        pdb_seq_id->SetMol(CPDB_mol_id(mol));

        if (rel) {
            CRef<CDate> date = MakeDatePtr(rel, source);
            pdb_seq_id->SetRel(*date);
        }

        CRef<CSeq_id> sid(new CSeq_id);
        sid->SetPdb(*pdb_seq_id);
        refs.push_back(sid);
    } else
        MakeChainPDBSeqId(refs, mol, chain);
}

/**********************************************************/
static void GetIntFuzzPtr(Uint1 choice, Int4 a, Int4 b, CInt_fuzz& fuzz)
{
    if (choice < 1 || choice > 4)
        return;

    if (choice == 2) {
        fuzz.SetRange().SetMax(a);
        if (b >= 0)
            fuzz.SetRange().SetMin(b);
    } else if (choice == 4) {
        fuzz.SetLim(static_cast<CInt_fuzz::ELim>(a));
    }
}

/**********************************************************/
static CBioSource::EGenome GetSPGenomeFrom_OS_OG(const TDataBlkList& dbl)
{
    char* p;
    Int4  gmod = -1;

    for (const auto& dbp : dbl)
        if (dbp.mType == ParFlatSP_OS) {
            for (const auto& subdbp : dbp.GetSubBlocks())
                if (subdbp.mType == ParFlatSP_OG) {
                    p = subdbp.mBuf.ptr + ParFlat_COL_DATA_SP;
                    if (StringEquNI(p, "Plastid;", 8))
                        for (p += 8; *p == ' ';)
                            p++;
                    gmod = StringMatchIcase(SP_organelle, p);
                }
        }
    if (gmod < 0)
        return CBioSource::eGenome_unknown;
    if (gmod == 0)
        return CBioSource::eGenome_chloroplast;
    if (gmod == 1)
        return CBioSource::eGenome_cyanelle;
    if (gmod == 2)
        return CBioSource::eGenome_mitochondrion;
    if (gmod == 3)
        return CBioSource::eGenome_plasmid;
    if (gmod == 4)
        return CBioSource::eGenome_nucleomorph;
    if (gmod == 5)
        return CBioSource::eGenome_hydrogenosome;
    if (gmod == 6)
        return CBioSource::eGenome_apicoplast;
    if (gmod == 7 || gmod == 8)
        return CBioSource::eGenome_chromatophore;
    return CBioSource::eGenome_unknown;
}

/**********************************************************/
static void SpAddToIndexBlk(const DataBlk& entry, IndexblkPtr pIndex)
{
    char*  eptr;
    char*  offset;
    size_t len = 0;

    if (! SrchNodeType(entry, ParFlatSP_ID, &len, &offset))
        return;

    eptr = offset + len - 1;
    if (len > 5 && StringEquN(eptr - 3, "AA.", 3))
        eptr -= 4;

    while (*eptr == ' ' && eptr > offset)
        eptr--;
    while (isdigit(*eptr) != 0 && eptr > offset)
        eptr--;
    pIndex->bases = fta_atoi(eptr + 1);
    while (*eptr == ' ' && eptr > offset)
        eptr--;
    if (*eptr == ';')
        eptr--;
    while (isalpha(*eptr) != 0 && eptr > offset)
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
static void GetSprotSubBlock(ParserPtr pp, const DataBlk& entry)
{
    DataBlk* dbp = TrackNodeType(entry, ParFlatSP_OS);
    if (dbp) {
        auto& os_blk = *dbp;
        BuildSubBlock(os_blk, ParFlatSP_OG, "OG");
        BuildSubBlock(os_blk, ParFlatSP_OC, "OC");
        BuildSubBlock(os_blk, ParFlatSP_OX, "OX");
        BuildSubBlock(os_blk, ParFlatSP_OH, "OH");
        GetLenSubNode(os_blk);
    }

    auto& chain = TrackNodes(entry);
    for (auto& ref_blk : chain) {
        if (ref_blk.mType != ParFlatSP_RN)
            continue;

        BuildSubBlock(ref_blk, ParFlatSP_RP, "RP");
        BuildSubBlock(ref_blk, ParFlatSP_RC, "RC");
        BuildSubBlock(ref_blk, ParFlatSP_RM, "RM");
        BuildSubBlock(ref_blk, ParFlatSP_RX, "RX");
        BuildSubBlock(ref_blk, ParFlatSP_RG, "RG");
        BuildSubBlock(ref_blk, ParFlatSP_RA, "RA");
        BuildSubBlock(ref_blk, ParFlatSP_RT, "RT");
        BuildSubBlock(ref_blk, ParFlatSP_RL, "RL");
        GetLenSubNode(ref_blk);
        ref_blk.mType = ParFlat_REF_END; /* swiss-prot only has one type */
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
static string GetSPDescrTitle(string_view sv, bool* fragment)
{
    const char* tag;
    char*       ptr;
    char*       str;
    char*       p;
    char*       q;
    Char        symb;
    Int4        shift;
    bool        ret;

    string str_ = GetBlkDataReplaceNewLine(sv, ParFlat_COL_DATA_SP);
    StripECO(str_);

    if (str_.find("(GENE NAME") != string::npos) {
        FtaErrPost(SEV_WARNING, ERR_GENENAME_DELineGeneName, "Old format, found gene_name in the DE data line");
    }

    ShrinkSpaces(str_);

    /* Delete (EC ...)
     */
    if (NStr::StartsWith(str_, "RecName: "sv, NStr::eNocase) ||
        NStr::StartsWith(str_, "AltName: "sv, NStr::eNocase) ||
        NStr::StartsWith(str_, "SubName: "sv, NStr::eNocase)) {
        tag   = "; EC=";
        symb  = ';';
        shift = 5;
    } else {
        tag   = "(EC ";
        symb  = ')';
        shift = 4;
    }

    str = StringSave(str_);
    for (ptr = str;;) {
        ptr = StringStr(ptr, tag);
        if (! ptr)
            break;

        for (p = ptr + shift; *p == ' ';)
            p++;

        if (*p == symb || *p == '\0') {
            ptr = p;
            continue;
        }

        while (*p == '.' || *p == '-' || *p == 'n' || isdigit(*p) != 0)
            p++;
        if (symb == ')')
            while (*p == ' ' || *p == ')')
                p++;

        fta_StringCpy(ptr, p);
    }

    if (symb == ';') {
        for (ptr = str;;) {
            ptr = StringIStr(ptr, "; Flags:");
            if (! ptr)
                break;
            if (ptr[8] == '\0') {
                *ptr = '\0';
                break;
            }
            if (ptr[8] != ' ') {
                ptr += 8;
                continue;
                ;
            }
            for (q = ptr + 8;;) {
                p = StringChr(q, ':');
                q = StringIStr(q, " Fragment");
                if (! q || (p && q > p))
                    break;

                ret = true;
                if (q[9] == ';')
                    fta_StringCpy(q, q + 10);
                else if (q[9] == '\0')
                    *q = '\0';
                else if (q[9] == 's' || q[9] == 'S') {
                    if (q[10] == ';')
                        fta_StringCpy(q, q + 11);
                    else if (q[10] == '\0')
                        *q = '\0';
                    else {
                        q++;
                        ret = false;
                    }
                } else {
                    q++;
                    ret = false;
                }
                if (ret)
                    *fragment = true;
            }
            if (ptr[8] == '\0') {
                *ptr = '\0';
                break;
            }
            q = StringChr(ptr + 8, ';');
            p = StringChr(ptr + 8, ':');
            if (! q) {
                if (! p)
                    break;
                else
                    fta_StringCpy(ptr + 2, ptr + 9);
            } else {
                if (! p)
                    ptr += 9;
                else {
                    if (p < q)
                        fta_StringCpy(ptr + 2, ptr + 9);
                    else
                        ptr += 9;
                }
            }
        }
    } else {
        ptr = StringIStr(str, "(FRAGMENT");
        if (ptr) {
            /* delete (FRAGMENTS) or (FRAGMENT)
             */
            *fragment = true;

            for (p = ptr + 8; *p != '\0' && *p != ')';)
                p++;
            while (*p == ' ' || *p == ')')
                p++;

            fta_StringCpy(ptr, p);
        }
    }

    string s = tata_save(str);
    MemFree(str);
    if (! s.empty() && s.back() == '.') {
        s.pop_back();
        while (! s.empty() && s.back() == ' ')
            s.pop_back();
        s.push_back('.');
    }
    return s;
}

/**********************************************************/
static char* GetLineOSorOC(const DataBlk& dbp, const char* pattern)
{
    char* res;
    char* p;
    char* q;

    size_t len = dbp.mBuf.len;
    if (len == 0)
        return nullptr;
    for (size_t i = 0; i < dbp.mBuf.len; i++)
        if (dbp.mBuf.ptr[i] == '\n')
            len -= 5;
    res = StringNew(len - 1);
    p   = res;
    for (q = dbp.mBuf.ptr; *q != '\0';) {
        if (! StringEquN(q, pattern, 5))
            break;
        if (p > res)
            *p++ = ' ';
        for (q += 5; *q != '\n' && *q != '\0'; q++)
            *p++ = *q;
        if (*q == '\n')
            q++;
    }
    *p = '\0';
    if (p > res)
        p--;
    while (*p == '.' || *p == ' ' || *p == '\t') {
        *p = '\0';
        if (p > res)
            p--;
    }
    return (res);
}

/**********************************************************/
static SetOfSpeciesPtr GetSetOfSpecies(char* line)
{
    SetOfSpeciesPtr res;
    SetOfSynsPtr    ssp;
    SetOfSynsPtr    tssp;
    char*           p;
    char*           q;
    char*           r;
    char*           temp;
    Int2            i;

    if (! line || line[0] == '\0')
        return nullptr;
    for (p = line; *p == ' ' || *p == '\t' || *p == '.' || *p == ',';)
        p++;
    if (*p == '\0')
        return nullptr;

    res           = new SetOfSpecies;
    res->fullname = StringSave(p);

    temp = StringSave(res->fullname);
    p    = StringChr(temp, '(');
    if (! p)
        res->name = StringSave(temp);
    else {
        *p = '\0';
        q  = temp;
        if (p > q) {
            for (r = p - 1; *r == ' ' || *r == '\t'; r--) {
                *r = '\0';
                if (r == q)
                    break;
            }
        }
        res->name = StringSave(temp);
        *p        = '(';
        ssp       = new SetOfSyns;
        tssp      = ssp;
        for (;;) {
            for (p++; *p == ' ' || *p == '\t';)
                p++;
            q = p;
            for (i = 1; *p != '\0'; p++) {
                if (*p == '(')
                    i++;
                else if (*p == ')')
                    i--;
                if (i == 0)
                    break;
            }
            if (*p == '\0') {
                tssp->next    = new SetOfSyns;
                tssp          = tssp->next;
                tssp->synname = StringSave(q);
                break;
            }
            *p = '\0';
            if (p > q) {
                for (r = p - 1; *r == ' ' || *r == '\t'; r--) {
                    *r = '\0';
                    if (r == q)
                        break;
                }
            }
            tssp->next    = new SetOfSyns;
            tssp          = tssp->next;
            tssp->synname = StringSave(q);
            *p            = ')';
            p             = StringChr(p, '(');
            if (! p)
                break;
        }

        res->syn = ssp->next;
        delete ssp;
    }

    MemFree(temp);
    return (res);
}

/**********************************************************/
static void fix_taxname_dot(COrg_ref& org_ref)
{
    if (! org_ref.IsSetTaxname())
        return;

    string& taxname = org_ref.SetTaxname();

    size_t len = taxname.size();
    if (len < 3)
        return;

    const Char* p = taxname.c_str() + len - 3;
    if ((p[0] == ' ' || p[0] == '\t') && (p[1] == 's' || p[1] == 'S') &&
        (p[2] == 'p' || p[2] == 'P') && p[3] == '\0') {
        if (NStr::EqualNocase(taxname, "BACTERIOPHAGE SP"sv))
            return;

        taxname += ".";
    }
}

/**********************************************************/
static CRef<COrg_ref> fill_orgref(SetOfSpeciesPtr sosp)
{
    SetOfSynsPtr synsp;

    const char** b;

    char*  p;
    char*  q;
    Uint1  num;
    size_t i;

    CRef<COrg_ref> org_ref;

    if (! sosp)
        return org_ref;

    org_ref.Reset(new COrg_ref);

    if (sosp->name && sosp->name[0] != '\0')
        org_ref->SetTaxname(sosp->name);

    for (synsp = sosp->syn; synsp; synsp = synsp->next) {
        p = synsp->synname;
        if (! p || *p == '\0')
            continue;

        q = StringIStr(p, "PLASMID");
        if (! q)
            q = StringIStr(p, "CLONE");
        if (q) {
            i = (*q == 'C' || *q == 'c') ? 5 : 7;
            if (q > p) {
                q--;
                i++;
            }
            if ((q == p || q[0] == ' ' || q[0] == '\t') &&
                (q[i] == ' ' || q[i] == '\t' || q[i] == '\0')) {
                if (! org_ref->IsSetTaxname())
                    org_ref->SetTaxname(p);
                else {
                    string& taxname = org_ref->SetTaxname();
                    taxname += " (";
                    taxname += p;
                    taxname += ")";
                }
                continue;
            }
        }

        if ((StringEquNI(p, "PV.", 3) && (p[3] == ' ' || p[3] == '\t' || p[3] == '\0')) ||
            NStr::EqualNocase(p, "AD11A") || NStr::EqualNocase(p, "AD11P")) {
            if (! org_ref->IsSetTaxname())
                org_ref->SetTaxname(p);
            else {
                string& taxname = org_ref->SetTaxname();
                taxname += " (";
                taxname += p;
                taxname += ")";
            }
            continue;
        }

        for (q = p; *p != '\0' && *p != ' ' && *p != '\t';)
            p++;
        if (*p == '\0') {
            org_ref->SetSyn().push_back(q);
            continue;
        }

        *p = '\0';
        for (q = p + 1; *q == ' ' || *q == '\t';)
            q++;

        if (NStr::EqualNocase(synsp->synname, "COMMON")) {
            if (! org_ref->IsSetCommon())
                org_ref->SetCommon(q);
            else
                org_ref->SetSyn().push_back(q);
            continue;
        }

        for (b = org_mods, num = 2; *b; b++, num++)
            if (NStr::EqualNocase(synsp->synname, *b))
                break;
        *p = ' ';

        if (! *b) {
            for (b = org_mods, num = 2; *b; b++, num++) {
                if (! NStr::EqualNocase(*b, "ISOLATE") &&
                    ! NStr::EqualNocase(*b, "STRAIN"))
                    continue;
                p = StringIStr(synsp->synname, *b);
                if (! p)
                    continue;

                p--;
                i = StringLen(*b) + 1;
                if (*p == ' ' && (p[i] == ' ' || p[i] == '\t' || p[i] == '\0')) {
                    string& taxname = org_ref->SetTaxname();
                    taxname += " (";
                    taxname += synsp->synname;
                    taxname += ")";
                    break;
                }
            }

            if (! *b)
                org_ref->SetSyn().push_back(synsp->synname);
            continue;
        }

        string& taxname = org_ref->SetTaxname();
        if (! taxname.empty())
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

    if (sosp->fullname)
        MemFree(sosp->fullname);
    if (sosp->name)
        MemFree(sosp->name);
    for (ssp = sosp->syn; ssp; ssp = tssp) {
        tssp = ssp->next;
        if (ssp->synname)
            MemFree(ssp->synname);
        delete ssp;
    }
    delete sosp;
}

/**********************************************************/
static ViralHostList GetViralHostsFrom_OH(DataBlkCIter dbp, DataBlkCIter dbp_end)
{
    char* line;
    char* p;
    char* q;
    char* r;
    Char  ch;

    for (; dbp != dbp_end; ++dbp)
        if (dbp->mType == ParFlatSP_OS)
            break;
    if (dbp == dbp_end)
        return {};

    const auto& subblocks = dbp->GetSubBlocks();
    auto        subdbp    = subblocks.cbegin();
    for (; subdbp != subblocks.cend(); ++subdbp)
        if (subdbp->mType == ParFlatSP_OH)
            break;
    if (subdbp == subblocks.cend())
        return {};

    ViralHostList vhl;
    auto          tvhp = vhl.before_begin();

    line                       = StringNew(subdbp->mBuf.len + 1);
    ch                         = subdbp->mBuf.ptr[subdbp->mBuf.len - 1];
    subdbp->mBuf.ptr[subdbp->mBuf.len - 1] = '\0';
    line[0]                    = '\n';
    line[1]                    = '\0';
    StringCat(line, subdbp->mBuf.ptr);
    subdbp->mBuf.ptr[subdbp->mBuf.len - 1] = ch;

    if (! StringEquNI(line, "\nOH   NCBI_TaxID=", 17)) {
        ch = '\0';
        p  = StringChr(line + 1, '\n');
        if (p)
            *p = '\0';
        if (StringLen(line + 1) > 20) {
            ch       = line[21];
            line[21] = '\0';
        }
        FtaErrPost(SEV_ERROR, ERR_SOURCE_UnknownOHType, "Unknown beginning of OH block: \"{}[...]\".", line + 1);
        if (ch != '\0')
            line[21] = ch;
        if (p)
            *p = '\n';
    }

    for (p = line;;) {
        p = StringIStr(p, "\nOH   NCBI_TaxID=");
        if (! p)
            break;
        for (p += 17, q = p; *q == ' ';)
            q++;
        r = StringChr(q, '\n');
        p = StringChr(q, ';');
        if ((! r || r > p) && p) {
            tvhp = vhl.emplace_after(tvhp);
            for (p--; *p == ';' || *p == ' ';)
                p--;
            p++;
            for (r = q; *r >= '0' && *r <= '9';)
                r++;
            *p = '\0';
            if (r != p) {
                FtaErrPost(SEV_ERROR, ERR_SOURCE_InvalidNcbiTaxID, "Invalid NCBI TaxID in OH line : \"{}\".", q);
                tvhp->taxid = ZERO_TAX_ID;
            } else
                tvhp->taxid = TAX_ID_FROM(int, fta_atoi(q));
            for (p++; *p == ' ' || *p == ';';)
                p++;
            r = StringChr(p, '\n');
            if (! r)
                r = p + StringLen(p);
            else
                r--;
            while ((*r == ' ' || *r == '.' || *r == '\0') && r > p)
                r--;
            if (*r != '\0' && *r != '.' && *r != ' ')
                r++;
            ch         = *r;
            *r         = '\0';
            tvhp->name = StringSave(p);
            ShrinkSpaces(tvhp->name);
            *r = ch;
            p  = r;
        } else {
            if (r)
                *r = '\0';
            FtaErrPost(SEV_ERROR, ERR_SOURCE_IncorrectOHLine, "Incorrect OH line content skipped: \"{}\".", q);
            if (r)
                *r = '\n';
            p = q;
        }
    }
    MemFree(line);

    if (vhl.empty())
        FtaErrPost(SEV_WARNING, ERR_SOURCE_NoNcbiTaxIDLookup, "No legal NCBI TaxIDs found in OH line.");

    return vhl;
}

/**********************************************************/
static TTaxId GetTaxIdFrom_OX(DataBlkCIter dbp, DataBlkCIter dbp_end)
{
    char*  line;
    char*  p;
    char*  q;
    bool   got   = false;
    TTaxId taxid = ZERO_TAX_ID;

    for (; dbp != dbp_end; ++dbp) {
        if (dbp->mType != ParFlatSP_OS)
            continue;

        for (const auto& subdbp : dbp->GetSubBlocks()) {
            if (subdbp.mType != ParFlatSP_OX)
                continue;
            got  = true;
            line = StringSave(string_view(subdbp.mBuf.ptr, subdbp.mBuf.len - 1));
            p    = StringChr(line, '\n');
            if (p)
                *p = '\0';
            if (! StringEquNI(line, "OX   NCBI_TaxID=", 16)) {
                if (StringLen(line) > 20)
                    line[20] = '\0';
                FtaErrPost(SEV_ERROR, ERR_SOURCE_UnknownOXType, "Unknown beginning of OX line: \"{}\".", line);
                MemFree(line);
                break;
            }
            p = StringChr(line + 16, ';');
            if (p) {
                *p++ = '\0';
                for (q = p; *q == ' ';)
                    q++;
                if (*q != '\0') {
                    FtaErrPost(SEV_ERROR, ERR_FORMAT_UnexpectedData, "Encountered unexpected data while parsing OX line: \"{}\" : Ignored.", p);
                }
            }
            for (p = line + 16; *p == ' ';)
                p++;
            if (*p == '\0') {
                MemFree(line);
                break;
            }
            for (q = p; *q >= '0' && *q <= '9';)
                q++;
            if (*q == ' ' || *q == '\0')
                taxid = TAX_ID_FROM(int, fta_atoi(p));
            if (taxid <= ZERO_TAX_ID || (*q != ' ' && *q != '\0')) {
                FtaErrPost(SEV_ERROR, ERR_SOURCE_InvalidNcbiTaxID, "Invalid NCBI TaxID on OX line : \"{}\" : Ignored.", p);
            }
            MemFree(line);
            break;
        }
        break;
    }

    if (got && taxid <= ZERO_TAX_ID)
        FtaErrPost(SEV_WARNING, ERR_SOURCE_NoNcbiTaxIDLookup, "No legal NCBI TaxID found on OX line : will use organism names for lookup instead.");

    return (taxid);
}

/**********************************************************/
static CRef<COrg_ref> GetOrganismFrom_OS_OC(DataBlkCIter entry, DataBlkCIter end)
{
    SetOfSpeciesPtr sosp;
    char*           line_OS;
    char*           line_OC;

    CRef<COrg_ref> org_ref;

    line_OS = nullptr;
    line_OC = nullptr;

    for (auto dbp = entry; dbp != end; ++dbp) {
        if (dbp->mType != ParFlatSP_OS)
            continue;
        line_OS = GetLineOSorOC(*dbp, "OS   ");
        for (const auto& subdbp : dbp->GetSubBlocks()) {
            if (subdbp.mType != ParFlatSP_OC)
                continue;
            line_OC = GetLineOSorOC(subdbp, "OC   ");
            break;
        }
        break;
    }

    if (line_OS && line_OS[0] != '\0') {
        sosp = GetSetOfSpecies(line_OS);
        if (sosp && sosp->name && sosp->name[0] != '\0') {
            org_ref = fill_orgref(sosp);
        }

        SetOfSpeciesFree(sosp);
        MemFree(line_OS);
    }

    if (org_ref.NotEmpty() && line_OC && line_OC[0] != '\0') {
        org_ref->SetOrgname().SetLineage(line_OC);
        MemFree(line_OC);
    }

    return org_ref;
}

/**********************************************************/
static void get_plasmid(const DataBlk& entry, CSP_block::TPlasnm& plasms)
{
    char* offset = nullptr;
    char* eptr   = nullptr;
    char* str;
    char* ptr;
    Int4  gmod = -1;

    const auto& chain = TrackNodes(entry);
    for (const auto& os_blk : chain) {
        if (os_blk.mType != ParFlatSP_OS)
            continue;

        for (const auto& subdbp : os_blk.GetSubBlocks()) {
            if (subdbp.mType != ParFlatSP_OG)
                continue;

            offset = subdbp.mBuf.ptr + ParFlat_COL_DATA_SP;
            eptr   = offset + subdbp.mBuf.len;
            gmod   = StringMatchIcase(SP_organelle, offset);
        }
    }
    if (gmod != Seq_descr_GIBB_mod_plasmid)
        return;

    while ((str = StringIStr(offset, "PLASMID"))) {
        if (str > eptr)
            break;

        str += StringLen("PLASMID");
        while (*str == ' ')
            str++;

        for (ptr = str; *ptr != '\n' && *ptr != ' ';)
            ptr++;
        ptr--;
        if (ptr > str) {
            plasms.push_back(string(str, ptr));
        } else
            FtaErrPost(SEV_ERROR, ERR_SOURCE_MissingPlasmidName, "Plasmid name is missing from OG line of SwissProt record.");
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
    if (! p || *p == '\0')
        return nullptr;

    for (;; p++) {
        if (*p == '\0' || *p == '\n')
            break;
        if ((*p == ';' || *p == '.') && (p[1] == ' ' || p[1] == '\n'))
            break;
    }

    if (*p == '\0' || *p == '\n')
        return nullptr;

    *p++ = '\0';

    ret = *ptr;

    while (*p == ' ' || *p == ';' || *p == '.')
        p++;
    *ptr = p;

    if (*ret == '\0')
        return nullptr;
    return (ret);
}

/**********************************************************/
static CRef<CSeq_id> AddPIDToSeqId(char* str, char* acc)
{
    long long lID;
    char*     end = nullptr;

    CRef<CSeq_id> sid;

    if (! str || *str == '\0')
        return sid;

    if (str[0] == '-') {
        FtaErrPost(SEV_WARNING, ERR_SPROT_DRLine, "Not annotated CDS [ACC:{}, PID:{}]", acc, str);
        return sid;
    }
    errno = 0; /* clear errors, the error flag from stdlib */
    lID   = strtoll(str + 1, &end, 10);
    if ((lID == 0 && str + 1 == end) || (lID == LLONG_MAX && errno == ERANGE)) {
        /* Bad or too large number
         */
        FtaErrPost(SEV_WARNING, ERR_SPROT_DRLine, "Invalid PID value [ACC:{}, PID:{}]", acc, str);
        return sid;
    }

    if (*str == 'G') {
        sid.Reset(new CSeq_id);
        sid->SetGi(GI_FROM(long long, lID));
    } else if (*str == 'E' || *str == 'D') {
        CRef<CDbtag> tag(new CDbtag);
        tag->SetDb("PID");
        tag->SetTag().SetStr(str);

        sid.Reset(new CSeq_id);
        sid->SetGeneral(*tag);
    } else {
        FtaErrPost(SEV_WARNING, ERR_SPROT_DRLine, "Unrecognized PID data base type [ACC:{}, PID:{}]", acc, str);
    }
    return sid;
}

/**********************************************************/
static bool AddToList(forward_list<string>& L, string_view str)
{
    if (str.empty())
        return false;

    if (str == "-"sv)
        return true;

    for (string_view it : L)
        if (it == str)
            return false;

    auto dot = str.find('.');
    if (dot != string_view::npos) {
        string_view acc2 = str.substr(0, dot);
        for (string_view it : L) {
            auto d = it.find('.');
            if (d != string_view::npos) {
                string_view acc1 = it.substr(0, d);
                if (acc1 == acc2) {
                    FtaErrPost(SEV_WARNING, ERR_SPROT_DRLine, "Same protein accessions with different versions found in DR line [PID1:{}.{}; PID2:{}.{}].", acc1, it.substr(d + 1), acc2, str.substr(dot + 1));
                }
            }
        }
    }

    auto tail = L.before_begin();
    while (next(tail) != L.end())
        ++tail;
    L.emplace_after(tail, str);

    return true;
}

/**********************************************************/
static void CheckSPDupPDBXrefs(CSP_block::TSeqref& refs)
{
    for (CSP_block::TSeqref::iterator cur_ref = refs.begin(); cur_ref != refs.end(); ++cur_ref) {
        if ((*cur_ref)->Which() != CSeq_id::e_Pdb || (*cur_ref)->GetPdb().IsSetRel())
            continue;

        bool got = false;

        const CPDB_seq_id&           cur_id   = (*cur_ref)->GetPdb();
        CSP_block::TSeqref::iterator next_ref = cur_ref;

        for (++next_ref; next_ref != refs.end();) {
            if ((*next_ref)->Which() != CSeq_id::e_Pdb ||
                (*next_ref)->GetPdb().IsSetRel())
                continue;

            const CPDB_seq_id& next_id = (*next_ref)->GetPdb();

            if (cur_id.GetMol().Get() != next_id.GetMol().Get()) {
                ++next_ref;
                continue;
            }

            if (next_id.GetChain() != 32) {
                if (! got && cur_id.GetChain() == 32) {
                    got = true;
                    /* Commented out until the proper handling of PDB chain contents
                    FtaErrPost(SEV_WARNING, ERR_FORMAT_DuplicateCrossRef,
                              "Duplicate PDB cross reference removed, mol = \"{}\", chain = \"{}\".",
                              psip1->mol, (int) psip1->chain);
*/
                }
                if (cur_id.GetChain() != next_id.GetChain()) {
                    ++next_ref;
                    continue;
                }
            }

            next_ref = refs.erase(next_ref);
            /* Commented out until the proper handling of PDB chain contents
            FtaErrPost(SEV_WARNING, ERR_FORMAT_DuplicateCrossRef,
                      "Duplicate PDB cross reference removed, mol = \"{}\", chain = \"{}\".",
                      psip2->mol, (int) psip2->chain);
*/
        }
    }
}

/**********************************************************/
static void fta_check_embl_drxref_dups(const TEmblAccList& embl_acc_list)
{
    if (embl_acc_list.empty() || next(embl_acc_list.cbegin()) == embl_acc_list.cend())
        return;

    for (auto it = embl_acc_list.cbegin(); it != embl_acc_list.cend(); ++it) {
        string_view pid = it->pid;
        auto        dot = pid.find('.');
        if (dot != string_view::npos) {
            for (auto p = pid.begin() + dot + 1; p != pid.end(); ++p) {
                if (*p >= '0' && *p <= '9')
                    continue;
                dot = string::npos;
                break;
            }
        }
        for (auto it2 = next(it); it2 != embl_acc_list.cend(); ++it2) {
            if (it->choice != it2->choice && pid == string_view(it2->pid)) {
                if (GetProtAccOwner(pid.substr(0, dot)) > CSeq_id::e_not_set)
                    FtaErrPost(SEV_WARNING, ERR_SPROT_DRLineCrossDBProtein, "Protein accession \"{}\" associated with \"{}\" and \"{}\".", pid, it->acc, it2->acc);
            }
        }
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
static void GetDRlineDataSP(const DataBlk& entry, CSP_block& spb, bool* drop, Parser::ESource source)
{
    forward_list<string> acc_list,
        pid_list,
        ens_tran_list,
        ens_prot_list,
        ens_gene_list;
    TEmblAccList embl_acc_list;
    const char** b;
    char*        offset;
    const char*  token1;
    char*        token2;
    char*        token3;
    char*        token4;
    char*        token5;
    char*        str;
    char*        ptr;
    char*        p;
    char*        q;
    bool         pdbold;
    bool         pdbnew;
    bool         check_embl_prot;
    size_t       len = 0;
    Char         ch;

    CSeq_id::E_Choice ptype;
    CSeq_id::E_Choice ntype;

    spb.ResetSeqref();
    spb.ResetDbref();

    if (! SrchNodeType(entry, ParFlatSP_DR, &len, &offset))
        return;

    ch          = offset[len];
    offset[len] = '\0';
    str         = StringNew(len + 1);
    StringCpy(str, "\n");
    StringCat(str, offset);
    offset[len]     = ch;
    pdbold          = false;
    pdbnew          = false;
    auto embl_tail  = embl_acc_list.before_begin();
    check_embl_prot = false;
    for (ptr = str;;) {
        if (*drop)
            break;
        ptr = StringChr(ptr, '\n');
        if (! ptr)
            break;
        ptr++;
        if (! fta_StartsWith(ptr, "DR   "sv))
            continue;
        ptr += ParFlat_COL_DATA_SP;
        token1 = GetDRToken(&ptr);
        token2 = GetDRToken(&ptr);
        token3 = GetDRToken(&ptr);
        token4 = GetDRToken(&ptr);
        token5 = GetDRToken(&ptr);
        if (! token1 || ! token2 || ! token3 ||
            (StringEqu(token2, "-") && StringEqu(token3, "-"))) {
            FtaErrPost(SEV_ERROR, ERR_SPROT_DRLine, "Badly formatted DR line. Skipped.");
            continue;
        }

        if (NStr::EqualNocase(token1, "MD5"))
            continue;

        for (b = valid_dbs; *b; b++)
            if (NStr::EqualNocase(*b, token1))
                break;
        if (! *b) {
            for (b = obsolete_dbs; *b; b++)
                if (NStr::EqualNocase(*b, token1))
                    break;
            if (! *b)
                FtaErrPost(SEV_WARNING, ERR_DRXREF_UnknownDBname, "Encountered a new/unknown database name in DR line: \"{}\".", token1);
            else
                FtaErrPost(SEV_WARNING, ERR_SPROT_DRLine, "Obsolete database name found in DR line: \"{}\".", token1);
        }

        if (NStr::EqualNocase(token1, "PDB")) {
            if (! token4)
                pdbold = true;
            else
                pdbnew = true;

            MakePDBSeqId(spb.SetSeqref(), token2, token3, token5 ? token5 : token4, drop, source);
        } else if (NStr::EqualNocase(token1, "PIR")) {
            CRef<CSeq_id> id(MakeLocusSeqId(token3, CSeq_id::e_Pir));
            if (id.NotEmpty())
                spb.SetSeqref().push_back(id);
        } else if (NStr::EqualNocase(token1, "EMBL")) {
            p = StringChr(token2, '.');
            ntype = GetNucAccOwner(p ? string_view(token2, p) : string_view(token2));
            if (ntype == CSeq_id::e_not_set) {
                FtaErrPost(SEV_ERROR, ERR_SPROT_DRLine, "Incorrect NA accession is used in DR line: \"{}\". Skipped...", token2);
            } else if (AddToList(acc_list, token2)) {
                CRef<CSeq_id> id(MakeAccSeqId(token2, ntype, p ? true : false,
                                              p ? (Int2) fta_atoi(p + 1) : 0));
                if (id.NotEmpty())
                    spb.SetSeqref().push_back(id);
            }
            if (p)
                *p = '\0';

            ptype = CSeq_id::e_not_set;
            if (token3[0] >= 'A' && token3[0] <= 'Z' &&
                token3[1] >= 'A' && token3[1] <= 'Z') {
                p = StringChr(token3, '.');
                if (p) {
                    ptype = GetProtAccOwner(string_view(token3, p));
                    for (q = p + 1; *q >= '0' && *q <= '9';)
                        q++;
                    if (q == p + 1 || *q != '\0')
                        p = nullptr;
                }
                if (! p || ptype == CSeq_id::e_not_set) {
                    FtaErrPost(SEV_ERROR, ERR_SPROT_DRLine, "Incorrect protein accession is used in DR line [ACC:{}; PID:{}]. Skipped...", token2, token3);
                    continue;
                }
            } else
                p = nullptr;

            if (ntype > CSeq_id::e_not_set) {
                embl_tail = embl_acc_list.emplace_after(embl_tail, ntype, token2, token3);
            }

            if (! AddToList(pid_list, token3)) {
                check_embl_prot = true;
                continue;
            }

            CRef<CSeq_id> id;
            if (! p)
                id = AddPIDToSeqId(token3, token2);
            else {
                *p++ = '\0';
                id   = MakeAccSeqId(token3, ptype, true, (Int2)fta_atoi(p));
            }

            if (id.NotEmpty())
                spb.SetSeqref().push_back(id);
        } else if (NStr::EqualNocase(token1, "ENSEMBL") ||
                   NStr::EqualNocase(token1, "ENSEMBLBACTERIA") ||
                   NStr::EqualNocase(token1, "ENSEMBLFUNGI") ||
                   NStr::EqualNocase(token1, "ENSEMBLMETAZOA") ||
                   NStr::EqualNocase(token1, "ENSEMBLPLANTS")  ||
                   NStr::EqualNocase(token1, "ENSEMBLPROTISTS") ||
                   NStr::EqualNocase(token1, "WORMBASE")) {
            if (AddToList(ens_tran_list, token2)) {
                CRef<CDbtag> tag = MakeStrDbtag(token1, token2);
                if (tag.NotEmpty())
                    spb.SetDbref().push_back(tag);
            }

            if (! AddToList(ens_prot_list, token3)) {
                FtaErrPost(SEV_WARNING, ERR_SPROT_DRLine, "Duplicated protein id \"{}\" in \"{}\" DR line.", token3, token1);
            } else {
                CRef<CDbtag> tag = MakeStrDbtag(token1, token3);
                if (tag.NotEmpty())
                    spb.SetDbref().push_back(tag);
            }

            if (token4 && AddToList(ens_gene_list, token4)) {
                CRef<CDbtag> tag = MakeStrDbtag(token1, token4);
                if (tag.NotEmpty())
                    spb.SetDbref().push_back(tag);
            }
        } else if (NStr::EqualNocase(token1, "REFSEQ")) {
            ptype = CSeq_id::e_not_set;
            if (token2[0] >= 'A' && token2[0] <= 'Z' &&
                token2[1] >= 'A' && token2[1] <= 'Z') {
                p = StringChr(token2, '.');
                if (p) {
                    ptype = GetProtAccOwner(string_view(token2, p));
                    for (q = p + 1; *q >= '0' && *q <= '9';)
                        q++;
                    if (q == p + 1 || *q != '\0')
                        p = nullptr;
                }
                if (ptype != CSeq_id::e_Other)
                    p = nullptr;
            } else
                p = nullptr;

            if (! p) {
                FtaErrPost(SEV_ERROR, ERR_SPROT_DRLine, "Incorrect protein accession.version is used in RefSeq DR line: \"{}\". Skipped...", token2);
                continue;
            }

            if (! AddToList(pid_list, token2))
                continue;

            *p++ = '\0';
            CRef<CSeq_id> id(MakeAccSeqId(token2, ptype, true, (Int2)fta_atoi(p)));
            if (id.NotEmpty())
                spb.SetSeqref().push_back(id);
        } else {
            if (NStr::EqualNocase(token1, "GK"))
                token1 = "Reactome";
            else if (NStr::EqualNocase(token1, "GENEW"))
                token1 = "HGNC";
            else if (NStr::EqualNocase(token1, "GeneDB_Spombe"))
                token1 = "PomBase";
            else if (NStr::EqualNocase(token1, "PomBase") &&
                     StringEquNI(token2, "PomBase:", 8))
                token2 += 8;

            CRef<CDbtag> tag = MakeStrDbtag(token1, token2);
            if (tag.NotEmpty()) {
                bool not_found = true;

                for (const auto& cur_tag : spb.SetDbref()) {
                    if (tag->Match(*cur_tag)) {
                        not_found = false;
                        break;
                    }
                }
                if (not_found)
                    spb.SetDbref().push_back(tag);
            }
        }
    }

    if (! embl_acc_list.empty()) {
        if (check_embl_prot)
            fta_check_embl_drxref_dups(embl_acc_list);
        embl_acc_list.clear();
    }

    acc_list.clear();
    pid_list.clear();
    ens_tran_list.clear();
    ens_prot_list.clear();
    ens_gene_list.clear();
    MemFree(str);

    if (pdbold && pdbnew) {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_MixedPDBXrefs, "Both old and new types of PDB cross-references exist on this record. Only one style is allowed.");
        *drop = true;
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
static bool GetSPDate(Parser::ESource source, const DataBlk& entry, CDate& crdate, CDate& sequpd, CDate& annotupd, short* ver_num)
{
    char*  offset;
    bool   new_style;
    size_t len;

    CRef<CDate_std> std_crdate,
        std_sequpd,
        std_annotupd;

    if (ver_num)
        *ver_num = 0;

    if (! SrchNodeType(entry, ParFlatSP_DT, &len, &offset))
        return true;
    string_view dtblock(offset, len);

    list<CTempString> dtlines;
    NStr::Split(dtblock, "\n", dtlines, NStr::fSplit_Truncate_End);

    int first  = 0;
    int second = 0;
    int third  = 0;
    if (CTempString::npos == dtlines.front().find('(')) {
        new_style = true;
        for (string_view line : dtlines) {
            line.remove_prefix(ParFlat_COL_DATA_SP);
            if (NPOS != NStr::FindNoCase(line, "integrated into")) {
                first++;
                std_crdate = GetUpdateDate(line, source);
            } else if (NPOS != NStr::FindNoCase(line, "entry version")) {
                third++;
                std_annotupd = GetUpdateDate(line, source);
            } else {
                auto pos = NStr::FindNoCase(line, "sequence version");
                if (pos != NPOS) {
                    static_assert("sequence version"sv.size() == 16);
                    pos += 16;
                    second++;
                    std_sequpd = GetUpdateDate(line, source);
                    if (ver_num) {
                        auto p = line.begin() + pos;
                        auto e = line.end();
                        while (p < e && *p == ' ')
                            p++;
                        auto q = p;
                        while (p < e && *p >= '0' && *p <= '9')
                            p++;
                        if (p + 1 == e && *p == '.') {
                            *ver_num = fta_atoi(string_view(q, p));
                        }
                    }
                }
            }
        }
    } else {
        new_style = false;
        for (string_view line : dtlines) {
            line.remove_prefix(ParFlat_COL_DATA_SP);
            if (NPOS != NStr::FindNoCase(line, "Created")) {
                first++;
                std_crdate = GetUpdateDate(line, source);
            } else if (NPOS != NStr::FindNoCase(line, "Last annotation update")) {
                third++;
                std_annotupd = GetUpdateDate(line, source);
            } else if (NPOS != NStr::FindNoCase(line, "Last sequence update")) {
                second++;
                std_sequpd = GetUpdateDate(line, source);
            }
        }
    }

    dtlines.clear();

    bool ret = true;
    if (first == 0) {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_Date, "Missing required \"{}\" DT line.", (new_style ? "integrated into" : "Created"));
        ret = false;
    } else if (first > 1) {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_Date, "Multiple \"{}\" DT lines are present.", (new_style ? "integrated into" : "Created"));
        ret = false;
    } else if (second == 0) {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_Date, "Missing required \"{}\" DT line.", (new_style ? "sequence version" : "Last sequence update"));
        ret = false;
    } else if (second > 1) {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_Date, "Multiple \"{}\" DT lines are present.", (new_style ? "sequence version" : "Last sequence update"));
        ret = false;
    } else if (third == 0) {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_Date, "Missing required \"{}\" DT line.", (new_style ? "entry version" : "Last annotation update"));
        ret = false;
    } else if (third > 1) {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_Date, "Multiple \"{}\" DT lines are present.", (new_style ? "entry version" : "Last annotation update"));
        ret = false;
    } else if (std_crdate.Empty()) {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_Date, "Missing or incorrect create date in \"{}\" DT line.", (new_style ? "integrated into" : "Created"));
        ret = false;
    } else if (std_sequpd.Empty()) {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_Date, "Missing or incorrect update date in \"{}\" DT line.", (new_style ? "sequence version" : "Last sequence update"));
        ret = false;
    } else if (std_annotupd.Empty()) {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_Date, "Missing or incorrect update date in \"{}\" DT line.", (new_style ? "entry version" : "Last annotation update"));
        ret = false;
    } else if (ver_num && *ver_num < 1) {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_Date, "Invalidly formatted sequence version DT line is present.");
        ret = false;
    }

    if (ret) {
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
static CRef<CSP_block>
GetDescrSPBlock(ParserPtr pp, const DataBlk& entry, CBioseq& bioseq)
{
    IndexblkPtr ibp;

    CRef<CSP_block> spb(new CSP_block);

    char* bptr;
    bool  reviewed;
    bool  i;
    Int2  ver_num;

    bptr = entry.mBuf.ptr + ParFlat_COL_DATA_SP;
    PointToNextToken(bptr); /* first ID line, 2nd token */
    reviewed = NStr::StartsWith(bptr, "reviewed"sv, NStr::eNocase);
    if (reviewed || NStr::StartsWith(bptr, "standard"sv, NStr::eNocase)) {
        spb->SetClass(CSP_block::eClass_standard);
    } else if (NStr::StartsWith(bptr, "preliminary"sv, NStr::eNocase) ||
               NStr::StartsWith(bptr, "unreviewed"sv, NStr::eNocase)) {
        spb->SetClass(CSP_block::eClass_prelim);
    } else {
        spb->SetClass(CSP_block::eClass_not_set);
        FtaErrPost(SEV_WARNING, ERR_DATACLASS_UnKnownClass, "Not a standard/reviewed or preliminary/unreviewed class in SWISS-PROT");
    }

    GetSequenceOfKeywords(entry, ParFlatSP_KW, ParFlat_COL_DATA_SP, spb->SetKeywords());

    ibp            = pp->entrylist[pp->curindx];
    ibp->wgssec[0] = '\0';

    GetExtraAccession(ibp, pp->allow_uwsec, pp->source, spb->SetExtra_acc());
    if (spb->SetExtra_acc().empty())
        spb->ResetExtra_acc();

    /* DT data ==> create-date, seqence update, annotation update
     */
    ver_num = 0;
    if (reviewed && pp->sp_dt_seq_ver)
        i = GetSPDate(pp->source, entry, spb->SetCreated(), spb->SetSequpd(), spb->SetAnnotupd(), &ver_num);
    else
        i = GetSPDate(pp->source, entry, spb->SetCreated(), spb->SetSequpd(), spb->SetAnnotupd(), nullptr);

    get_plasmid(entry, spb->SetPlasnm());
    if (spb->SetPlasnm().empty())
        spb->ResetPlasnm();

    GetDRlineDataSP(entry, *spb, &ibp->drop, pp->source);

    if (! i)
        ibp->drop = true;
    else if (spb->GetClass() == CSP_block::eClass_standard ||
             spb->GetClass() == CSP_block::eClass_prelim) {
        for (auto& cur_id : bioseq.SetId()) {
            if (! cur_id->IsSwissprot())
                continue;

            CTextseq_id& id = cur_id->SetSwissprot();
            if (ver_num > 0)
                id.SetVersion(ver_num);

            if (spb->GetClass() == CSP_block::eClass_standard)
                id.SetRelease("reviewed");
            else
                id.SetRelease("reviewed");

            break;
        }
    }

    CRef<CSeqdesc> descr(new CSeqdesc);
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
static void ParseSpComment(CSeq_descr::Tdata& descrs, char* line)
{
    char* com;
    char* p;
    char* q;
    Int2  i;

    for (p = line; *p == ' ';)
        p++;

    com = StringNew(StringLen(p) + 1);
    q   = com;
    i   = fta_StringMatch(ParFlat_SPComTopics, p);
    if (i >= 0)
        *q++ = '[';

    while (*p != '\0') {
        if (*p != '\n') {
            *q++ = *p++;
            continue;
        }

        if (p > line && *(p - 1) != '-')
            *q++ = ' ';
        for (++p; *p == ' ';)
            p++;
        if (StringEquN(p, "CC ", 3))
            for (p += 3; *p == ' ';)
                p++;
    }
    if (q == com) {
        MemFree(com);
        return;
    }
    for (--q; q > com && *q == ' ';)
        q--;
    if (*q != ' ')
        q++;
    *q = '\0';
    if (i >= 0) {
        p  = StringChr(com, ':');
        *p = ']';
    }

    if (com[0] != 0) {
        CRef<CSeqdesc> descr(new CSeqdesc);
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
static void GetSPDescrComment(const DataBlk& entry, CSeq_descr::Tdata& descrs, char* acc, Uint1 cla)
{
    char* offset;
    char* bptr;
    char* eptr;
    char* tmp;
    char* p;
    char* q;
    Char  ch;
    Int2  count;
    Int4  i;

    size_t len = 0;
    if (! SrchNodeType(entry, ParFlatSP_CC, &len, &offset))
        return;

    eptr  = offset + len;
    ch    = *eptr;
    *eptr = '\0';
    for (count = 0, p = offset;;) {
        p = StringStr(p, "----------");
        if (! p)
            break;
        for (q = p; q > offset && *q != '\n';)
            q--;
        if (*q == '\n')
            q++;

        p = StringChr(p, '\n');
        if (! p)
            break;
        for (i = 0; *p != '\0' && i < ParFlat_COL_DATA_SP + 1; i++)
            p++;
        if (*p == '\0')
            break;
        if (! NStr::StartsWith(p, COPYRIGHT, NStr::eNocase) &&
            ! NStr::StartsWith(p, COPYRIGHT1, NStr::eNocase))
            break;
        p = StringStr(p, "----------");
        if (! p)
            break;
        p = StringChr(p, '\n');
        if (! p)
            break;
        p++;
        len -= (p - q);
        fta_StringCpy(q, p);
        p = q;
        count++;
    }

    if (count == 0 && cla != 2) /* not PRELIMINARY or UNREVIEWED */
        FtaErrPost(SEV_WARNING, ERR_FORMAT_MissingCopyright, "The expected copyright notice for UniProt/Swiss-Prot entry {} was not found.", acc);

    if (len < 1) {
        *eptr = ch;
        return;
    }

    bptr = offset + ParFlat_COL_DATA_SP + 4;

    for (; (tmp = StringStr(bptr, "-!-")); bptr = tmp + 4) {
        /* found a new comment
         */
        for (p = tmp; p > bptr && *p != '\n';)
            p--;
        if (p == bptr)
            continue;
        *p = '\0';
        ParseSpComment(descrs, bptr);
        *p = '\n';
    }

    ParseSpComment(descrs, bptr);
    *eptr = ch;
}

/**********************************************************/
static void SPAppendPIRToHist(CBioseq& bioseq, const CSP_block& spb)
{
    if (spb.GetSeqref().empty())
        return;

    CSeq_hist_rec::TIds rep_ids;

    for (const auto& cur_ref : spb.GetSeqref()) {
        if (! cur_ref->IsPir())
            continue;

        CRef<CTextseq_id> text_id(new CTextseq_id);
        text_id->Assign(cur_ref->GetPir());

        CRef<CSeq_id> rep_id(new CSeq_id);
        rep_id->SetPir(*text_id);

        rep_ids.push_back(rep_id);
    }

    if (rep_ids.empty())
        return;

    CSeq_hist& hist = bioseq.SetInst().SetHist();
    hist.SetReplaces().SetIds().splice(hist.SetReplaces().SetIds().end(), rep_ids);
}

/**********************************************************/
static bool IfOHTaxIdMatchOHName(const char* orpname, const char* ohname)
{
    const char* p;
    const char* q;
    Char  chp;
    Char  chq;

    if (! orpname && ! ohname)
        return true;
    if (! orpname || ! ohname)
        return false;

    for (p = orpname, q = ohname; *p != '\0' && *q != '\0'; p++, q++) {
        chp = *p;
        if (chp >= 'a' && chp <= 'z')
            chp &= ~040;
        chq = *q;
        if (chq >= 'a' && chq <= 'z')
            chq &= ~040;
        if (chp != chq)
            break;
    }

    while (*p == ' ')
        p++;
    if (*p != '\0')
        return false;

    while (*q == ' ')
        q++;
    if (*q == '(' || *q == '\0')
        return true;
    return false;
}

/**********************************************************/
static void GetSprotDescr(CBioseq& bioseq, ParserPtr pp, const DataBlk& entry)
{
    char*      offset;
    CBioSource::TGenome gmod;
    bool       fragment = false;
    TTaxId     taxid;

    IndexblkPtr  ibp;

    CSeq_descr& descr = bioseq.SetDescr();

    ibp        = pp->entrylist[pp->curindx];
    size_t len = 0;
    if (SrchNodeType(entry, ParFlatSP_DE, &len, &offset)) {
        string title = GetSPDescrTitle(string_view(offset, len), &fragment);
        if (! title.empty()) {
            CRef<CSeqdesc> desc_new(new CSeqdesc);
            desc_new->SetTitle(title);
            descr.Set().push_back(desc_new);
        }
    }

    /* sp-block
     */
    CRef<CSP_block> spb = GetDescrSPBlock(pp, entry, bioseq);

    GetSPDescrComment(entry, descr.Set(), ibp->acnum, spb->GetClass());

    if (spb.NotEmpty() && pp->accver && pp->histacc && pp->source == Parser::ESource::SPROT) {
        CSeq_hist_rec::TIds rep_ids;

        for (const string& cur_acc : spb->GetExtra_acc()) {
            if (cur_acc.empty() || ! IsSPROTAccession(cur_acc.c_str()))
                continue;

            CRef<CTextseq_id> text_id(new CTextseq_id);
            text_id->SetAccession(cur_acc);

            CRef<CSeq_id> rep_id(new CSeq_id);
            rep_id->SetSwissprot(*text_id);
            rep_ids.push_back(rep_id);
        }

        if (! rep_ids.empty()) {
            CSeq_hist& hist = bioseq.SetInst().SetHist();
            hist.SetReplaces().SetIds().swap(rep_ids);
        }
    }

    if (spb->CanGetCreated()) {
        CRef<CSeqdesc> create_date_descr(new CSeqdesc);
        create_date_descr->SetCreate_date().Assign(spb->GetCreated());

        descr.Set().push_back(create_date_descr);
    }

    bool  has_update_date = spb->CanGetAnnotupd() || spb->CanGetSequpd();
    CDate upd_date;

    if (has_update_date) {
        if (spb->CanGetAnnotupd() && spb->CanGetSequpd()) {
            upd_date.Assign(spb->GetAnnotupd().Compare(spb->GetSequpd()) == CDate::eCompare_after ? spb->GetAnnotupd() : spb->GetSequpd());
        } else if (spb->CanGetAnnotupd())
            upd_date.Assign(spb->GetAnnotupd());
        else
            upd_date.Assign(spb->GetSequpd());

        CRef<CSeqdesc> upd_date_descr(new CSeqdesc);
        upd_date_descr->SetUpdate_date().Assign(upd_date);

        descr.Set().push_back(upd_date_descr);
    }

    if (spb->CanGetCreated() && has_update_date &&
        spb->GetCreated().Compare(upd_date) == CDate::eCompare_after) {
        string upd_date_str, create_date_str;

        upd_date.GetDate(&upd_date_str);
        spb->GetCreated().GetDate(&create_date_str);

        FtaErrPost(SEV_ERROR, ERR_DATE_IllegalDate, "Update-date \"{}\" precedes create-date \"{}\".", upd_date_str, create_date_str);
    }

    auto& chain = TrackNodes(entry);
    gmod = GetSPGenomeFrom_OS_OG(chain);

    /* Org-ref from ID lines
     */
    for (auto dbp = chain.cbegin(); dbp != chain.cend(); ++dbp) {
        if (dbp->mType != ParFlatSP_ID)
            continue;

        CRef<CBioSource> bio_src;

        taxid = GetTaxIdFrom_OX(dbp, chain.cend());
        if (taxid > ZERO_TAX_ID) {
            CRef<COrg_ref> org_ref = fta_fix_orgref_byid(pp, taxid, &ibp->drop, false);
            if (org_ref.Empty())
                FtaErrPost(SEV_ERROR, ERR_SOURCE_NcbiTaxIDLookupFailure, "NCBI TaxID lookup for {} failed : will use organism name for lookup instead.", TAX_ID_TO(int, taxid));
            else {
                bio_src.Reset(new CBioSource);

                if (gmod != CBioSource::eGenome_unknown)
                    bio_src->SetGenome(gmod);
                bio_src->SetOrg(*org_ref);
            }
        }

        CRef<COrg_ref> org_ref = GetOrganismFrom_OS_OC(dbp, chain.cend());
        if (org_ref.NotEmpty()) {
            if (bio_src.Empty()) {
                bio_src.Reset(new CBioSource);

                if (gmod != CBioSource::eGenome_unknown)
                    bio_src->SetGenome(gmod);
                fta_fix_orgref(pp, *org_ref, &ibp->drop, nullptr);
                bio_src->SetOrg(*org_ref);
            } else if (org_ref->IsSetTaxname()) {
                if (! bio_src->IsSetOrg() || ! bio_src->GetOrg().IsSetTaxname() ||
                    ! NStr::EqualNocase(org_ref->GetTaxname(), bio_src->GetOrg().GetTaxname()))
                    FtaErrPost(SEV_ERROR, ERR_SOURCE_OrgNameVsTaxIDMissMatch, "Organism name \"{}\" from OS line does not match the organism name \"{}\" obtained by lookup of NCBI TaxID.", org_ref->GetTaxname(), bio_src->GetOrg().GetTaxname());
            }
        }

        if (bio_src.Empty())
            break;

        ViralHostList vhl = GetViralHostsFrom_OH(dbp, chain.cend());
        if (! vhl.empty()) {
            COrgName& orgname = bio_src->SetOrg().SetOrgname();

            for (; ! vhl.empty(); vhl.pop_front()) {
                const auto& vh = vhl.front();

                CRef<COrgMod> mod(new COrgMod);
                mod->SetSubtype(COrgMod::eSubtype_nat_host);
                mod->SetSubname(vh.name);
                orgname.SetMod().push_back(mod);

                if (vh.taxid <= ZERO_TAX_ID) {
                    continue;
                }

                bool drop = false;
                CRef<COrg_ref> org_ref_cur = fta_fix_orgref_byid(pp, vh.taxid, &drop, true);
                if (org_ref_cur.Empty()) {
                    if (! drop)
                        FtaErrPost(SEV_ERROR, ERR_SOURCE_InvalidNcbiTaxID, "OH-line TaxId \"{}\" was not found via the NCBI TaxArch service.", TAX_ID_TO(int, vh.taxid));
                    else
                        FtaErrPost(SEV_ERROR, ERR_SOURCE_NcbiTaxIDLookupFailure, "Taxonomy lookup for OH-line TaxId \"{}\" failed.", TAX_ID_TO(int, vh.taxid));
                } else {
                    vector<Char> org_taxname;
                    if (org_ref_cur->IsSetTaxname()) {
                        const string& cur_taxname = org_ref_cur->GetTaxname();
                        org_taxname.assign(cur_taxname.begin(), cur_taxname.end());
                    }

                    org_taxname.push_back(0);

                    if (! IfOHTaxIdMatchOHName(&org_taxname[0], vh.name))
                        FtaErrPost(SEV_WARNING,
                                   ERR_SOURCE_HostNameVsTaxIDMissMatch,
                                   "OH-line HostName \"{}\" does not match NCBI organism name \"{}\" obtained by lookup of NCBI TaxID \"{}\".",
                                   vh.name,
                                   &org_taxname[0],
                                   TAX_ID_TO(int, vh.taxid));
                }
            }
        }

        fta_sort_biosource(*bio_src);

        CRef<CSeqdesc> bio_src_desc(new CSeqdesc);
        bio_src_desc->SetSource(*bio_src);
        descr.Set().push_back(bio_src_desc);
        break;
    }

    if (spb.NotEmpty())
        SPAppendPIRToHist(bioseq, *spb);

    CRef<CSeqdesc> mol_info_descr(new CSeqdesc);
    CMolInfo&      mol_info = mol_info_descr->SetMolinfo();
    mol_info.SetBiomol(CMolInfo::eBiomol_peptide);
    mol_info.SetCompleteness(fragment ? CMolInfo::eCompleteness_partial : CMolInfo::eCompleteness_complete);

    descr.Set().push_back(mol_info_descr);

    /* RN data ==> pub
     */
    for (auto& ref_blk : chain) {
        if (ref_blk.mType != ParFlat_REF_END)
            continue;

        CRef<CPubdesc> pub_desc = DescrRefs(pp, ref_blk, ParFlat_COL_DATA_SP);
        if (pub_desc.NotEmpty()) {
            CRef<CSeqdesc> pub_desc_descr(new CSeqdesc);
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
static void GetSPInst(ParserPtr pp, const DataBlk& entry, unsigned char* protconv)
{
    EntryBlkPtr ebp;

    ebp = entry.GetEntryData();

    CBioseq& bioseq = ebp->seq_entry->SetSeq();

    bioseq.SetInst().SetRepr(CSeq_inst::eRepr_raw);
    bioseq.SetInst().SetMol(CSeq_inst::eMol_aa);

    GetSeqData(pp, entry, bioseq, ParFlatSP_SQ, protconv, CSeq_data::e_Iupacaa);
}

/**********************************************************/
static bool fta_spfeats_same(SPFeatInput& fi1, SPFeatInput& fi2)
{
    if (fi1.key != fi2.key ||
        fi1.from != fi2.from ||
        fi1.to != fi2.to ||
        fi1.descrip != fi2.descrip)
        return false;

    return true;
}

/**********************************************************/
static void fta_remove_dup_spfeats(SPFeatInputList& spfil)
{
    if (spfil.empty() || next(spfil.begin()) == spfil.end())
        return;

    for (auto spfip = spfil.begin(); spfip != spfil.end() && next(spfip) != spfil.end(); ++spfip) {
        auto fipprev = spfip;
        for (auto fip = next(fipprev); fip != spfil.end();) {
            if (! fta_spfeats_same(*spfip, *fip)) {
                fipprev = fip++;
                continue;
            }
            FtaErrPost(SEV_WARNING, ERR_FEATURE_DuplicateRemoved, "Duplicated feature \"{}\" at location \"{}..{}\" removed.", fip->key.empty() ? "???" : fip->key, fip->from.empty() ? "???" : fip->from, fip->to.empty() ? "???" : fip->to);
            fip = spfil.erase_after(fipprev);
        }
    }
}

/**********************************************************/
static void SPPostProcVarSeq(string& varseq)
{
    char* temp;
    char* end;
    char* p;
    char* q;

    if (varseq.empty())
        return;

    temp = StringSave(varseq);
    p    = StringStr(temp, "->");
    if (! p || p == temp ||
        (*(p - 1) != ' ' && *(p - 1) != '\n') || (p[2] != ' ' && p[2] != '\n')) {
        NStr::ReplaceInPlace(varseq, "\n", " ");
        MemFree(temp);
        return;
    }

    for (p--; p > temp && (*p == ' ' || *p == '\n');)
        p--;
    if (*p < 'A' || *p > 'Z') {
        NStr::ReplaceInPlace(varseq, "\n", " ");
        MemFree(temp);
        return;
    }

    end = p + 1;
    while (p > temp && (*p == '\n' || (*p >= 'A' && *p <= 'Z')))
        p--;
    if (p > temp)
        p++;
    while (*p == '\n')
        p++;
    for (;;) {
        while (*p >= 'A' && *p <= 'Z' && p < end)
            p++;
        if (p == end)
            break;
        for (q = p; *p == '\n'; p++)
            end--;
        fta_StringCpy(q, p);
    }

    while (*p == ' ' || *p == '\n')
        p++;
    for (p += 2; *p == ' ' || *p == '\n';)
        p++;

    if (*p < 'A' || *p > 'Z') {
        NStr::ReplaceInPlace(varseq, "\n", " ");
        MemFree(temp);
        return;
    }

    for (q = p; *q == '\n' || (*q >= 'A' && *q <= 'Z');)
        q++;
    if (q > p && *(q - 1) == '\n') {
        for (q--; *q == '\n' && q > p;)
            q--;
        if (*q != '\n')
            q++;
    }
    end = q;

    for (;;) {
        while (*p >= 'A' && *p <= 'Z' && p < end)
            p++;
        if (p == end)
            break;
        for (q = p; *p == '\n'; p++)
            end--;
        fta_StringCpy(q, p);
    }

    for (p = temp; *p != '\0'; p++)
        if (*p == '\n')
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
static SPFeatInputList ParseSPFeat(const DataBlk& entry, size_t seqlen)
{
    SPFeatInputList spfil;
    const char*    defdelim;
    char*          fromstart;
    char*          fromend;
    char*          bptr;
    char*          eptr;
    char*          ptr1;
    char*          offset;
    char*          endline;
    char*          str;
    const char*    delim;
    char*          quotes;
    char*          location;
    char*          p;
    char*          q;
    int            i;
    bool           badqual;
    bool           new_format;
    bool           extra_text;
    Char           ch;

    size_t len = 0;
    if (! SrchNodeType(entry, ParFlatSP_FT, &len, &offset))
        return spfil;

    bptr = offset + ParFlat_COL_DATA_SP;
    eptr = offset + len;

    auto current = spfil.before_begin();

    while (bptr < eptr && (endline = SrchTheChar(string_view(bptr, eptr), '\n'))) {
        SPFeatInput temp;

        for (p = bptr, i = 0; *p != ' ' && *p != '\n' && i < 8; i++)
            p++;
        temp.key.assign(bptr, p);
        NStr::TruncateSpacesInPlace(temp.key, NStr::eTrunc_End);

        if (temp.key == "VAR_SEQ")
            defdelim = "\n";
        else
            defdelim = " ";

        for (bptr += 8; *bptr == ' ' && bptr <= endline;)
            bptr++;

        location = bptr;

        if (((*bptr >= 'a' && *bptr <= 'z') || (*bptr >= 'A' && *bptr <= 'Z')) &&
            bptr[6] == '-') {
            for (bptr += 7; *bptr >= '0' && *bptr <= '9' && bptr <= endline;)
                bptr++;
            for (; *bptr == ':' && bptr <= endline;)
                bptr++;
        }

        for (ptr1 = bptr; *ptr1 == '?' || *ptr1 == '>' || *ptr1 == '<' ||
                          (*ptr1 >= '0' && *ptr1 <= '9');)
            ptr1++;

        if (bptr < ptr1 && ptr1 <= endline) {
            temp.from.assign(bptr, ptr1);
            fromstart = bptr;
            fromend   = ptr1;
        } else {
            ch = '\0';
            p  = StringChr(location, ' ');
            q  = StringChr(location, '\n');
            if (! p || (q && q < p))
                p = q;
            if (p) {
                ch = *p;
                *p = '\0';
            }
            if (bptr == ptr1)
                FtaErrPost(SEV_ERROR, ERR_FEATURE_BadLocation, "Invalid location \"{}\" at feature \"{}\". Feature dropped.", location, temp.key);
            else
                FtaErrPost(SEV_ERROR, ERR_FEATURE_BadLocation, "Empty location at feature \"{}\". Feature dropped.", temp.key);
            if (p)
                *p = ch;
            temp.from.assign("-1");
            fromstart = nullptr;
            fromend   = nullptr;
        }

        new_format = false;
        bptr       = ptr1;
        for (; (*bptr == ' ' || *bptr == '.') && bptr <= endline; bptr++)
            if (*bptr == '.')
                new_format = true;
        for (ptr1 = bptr; *ptr1 == '?' || *ptr1 == '>' || *ptr1 == '<' ||
                          (*ptr1 >= '0' && *ptr1 <= '9');)
            ptr1++;

        p = (char*)temp.from.c_str();
        if (*p == '<' || *p == '>')
            p++;

        for (q = ptr1; *q == ' ';)
            q++;
        extra_text = false;
        if (bptr < ptr1 && ptr1 <= endline) {
            if (*q != '\n' && new_format && (*p == '?' || fta_atoi(p) != -1))
                extra_text = true;
            temp.to.assign(bptr, ptr1);
        } else if (fromstart) {
            if (*q != '\n' && (*p == '?' || fta_atoi(p) != -1))
                extra_text = true;
            temp.to.assign(fromstart, fromend);
        } else {
            if (*q != '\n' && (*p == '?' || fta_atoi(p) != -1))
                extra_text = true;
            temp.to.assign("-1");
        }

        q = (char*)temp.to.c_str();
        if (*q == '<' || *q == '>')
            q++;
        if (extra_text || (*p != '?' && *q != '?' && (fta_atoi(p) > fta_atoi(q)))) {
            ch = '\0';
            p  = extra_text ? nullptr : StringChr(location, ' ');
            q  = StringChr(location, '\n');
            if (! p || (q && q < p))
                p = q;
            if (p) {
                ch = *p;
                *p = '\0';
            }
            FtaErrPost(SEV_ERROR, ERR_FEATURE_BadLocation, "Invalid location \"{}\" at feature \"{}\". Feature dropped.", location, temp.key);
            if (p)
                *p = ch;
            temp.from.assign("-1");
        }

        for (bptr = ptr1; *bptr == ' ' && bptr <= endline;)
            bptr++;

        str   = endline;
        delim = defdelim;
        if (str > bptr)
            if (*--str == '-' && str > bptr)
                if (*--str != ' ')
                    delim = nullptr;
        if (bptr <= endline)
            temp.descrip.assign(bptr, endline);

        for (bptr = endline; *bptr == ' ' || *bptr == '\n';)
            bptr++;

        badqual = false;
        bptr += ParFlat_COL_DATA_SP;
        while (bptr < eptr && (*bptr == ' ')) /* continue description data */
        {
            while (*bptr == ' ')
                bptr++;

            if (StringEquN(bptr, "/note=\"", 7)) {
                bptr += 7;
                quotes = nullptr;
            } else if (StringEquN(bptr, "/evidence=\"", 11)) {
                quotes = bptr + 10;
                if (! StringEquN(quotes + 1, "ECO:", 4)) {
                    p = StringChr(bptr, '\n');
                    if (p)
                        *p = '\0';
                    FtaErrPost(SEV_ERROR, ERR_QUALIFIER_InvalidEvidence, "/evidence qualifier does not have expected \"ECO:\" prefix : \"{}\".", bptr);
                    if (p)
                        *p = '\n';
                }
            } else if (StringEquN(bptr, "/id=\"", 5))
                quotes = bptr + 4;
            else {
                if (*bptr == '/') {
                    for (p = bptr + 1; (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_';)
                        p++;
                    if (*p == '=' && p[1] == '\"') {
                        *p      = '\0';
                        badqual = true;
                        FtaErrPost(SEV_ERROR, ERR_FEATURE_InvalidQualifier, "Qualifier {} is invalid for the feature \"{}\" at \"{}..{}\".", bptr, temp.key, temp.from, temp.to);
                        *p = '=';
                    }
                }
                quotes = nullptr;
            }

            endline = SrchTheChar(string_view(bptr, eptr), '\n');
            p       = endline - 1;
            if (p >= bptr && *p == '\"')
                *p = '.';
            else
                p = nullptr;

            if (quotes) {
                StringCombine(temp.descrip, string(bptr, quotes), delim);
                if (p && p - 1 >= bptr && *(p - 1) == '.')
                    StringCombine(temp.descrip, string(quotes + 1, endline - 1), "");
                else
                    StringCombine(temp.descrip, string(quotes + 1, endline), "");
            } else {
                if (p && p - 1 >= bptr && *(p - 1) == '.')
                    StringCombine(temp.descrip, string(bptr, endline - 1), delim);
                else
                    StringCombine(temp.descrip, string(bptr, endline), delim);
            }

            if (p)
                *p = '\"';

            str   = endline;
            delim = defdelim;
            if (str > bptr)
                if (*--str == '-' && str > bptr)
                    if (*--str != ' ')
                        delim = nullptr;
            for (bptr = endline; *bptr == ' ' || *bptr == '\n';)
                bptr++;

            bptr += ParFlat_COL_DATA_SP;
        }

        if (badqual) {
            FtaErrPost(SEV_ERROR, ERR_FEATURE_Dropped, "Invalid qualifier(s) found within the feature \"{}\" at \"{}..{}\". Feature dropped.", temp.key, temp.from, temp.to);
            continue;
        }

        if (*defdelim == '\n')
            SPPostProcVarSeq(temp.descrip);

        p = (char*)temp.from.c_str();
        if (*p == '<' || *p == '>')
            p++;
        if (*p != '?' && fta_atoi(p) < 0) {
            continue;
        }

        q = (char*)temp.to.c_str();
        if (*q == '<' || *q == '>')
            q++;
        if ((*p != '?' && fta_atoi(p) > (Int4)seqlen) || (*q != '?' && fta_atoi(q) > (Int4)seqlen)) {
            FtaErrPost(SEV_WARNING, ERR_LOCATION_FailedCheck, "Location range exceeds the sequence length: feature={}, length={}, from={}, to={}", temp.key, seqlen, temp.from, temp.to);
            FtaErrPost(SEV_ERROR, ERR_FEATURE_Dropped, "Location range exceeds the sequence length: feature={}, length={}, from={}, to={}", temp.key, seqlen, temp.from, temp.to);
            continue;
        }

        current = spfil.insert_after(current, temp);
    }

    fta_remove_dup_spfeats(spfil);

    return spfil;
}

/**********************************************************
 *
 *   static CRef<CSeq_loc> GetSPSeqLoc(pp, spfip, bond, initmet,
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
static CRef<CSeq_loc> GetSPSeqLoc(ParserPtr pp, const SPFeatInput& spfip, bool bond, bool initmet, bool signal)
{
    CRef<CSeq_loc> loc;

    IndexblkPtr ibp;

    const char* ptr;

    bool fuzzfrom = false;
    bool fuzzto   = false;
    bool nofrom   = false;
    bool noto     = false;
    bool pntfuzz  = false;
    Int4 from;
    Int4 to;

    if (spfip.from.empty() || spfip.to.empty())
        return loc;

    ibp = pp->entrylist[pp->curindx];

    loc.Reset(new CSeq_loc);

    ptr = spfip.from.c_str();
    if (StringChr(ptr, '<')) {
        fuzzfrom = true;

        while (*ptr != '\0' && isdigit(*ptr) == 0)
            ptr++;
        from = (Int4)fta_atoi(ptr);
    } else if (StringChr(ptr, '?')) {
        from   = 0;
        nofrom = true;
    } else {
        from = (Int4)fta_atoi(ptr);
    }
    if ((initmet == false && from != 0) ||
        (initmet && signal && from == 1))
        from--;

    ptr = spfip.to.c_str();
    if (StringChr(ptr, '>')) {
        fuzzto = true;
        while (*ptr != '\0' && isdigit(*ptr) == 0)
            ptr++;
        to = (Int4)fta_atoi(ptr);
    } else if (StringChr(ptr, '?')) {
        to   = static_cast<Int4>(ibp->bases);
        noto = true;
    } else
        to = (Int4)fta_atoi(ptr);

    if (initmet == false && to != 0)
        to--;
    if (nofrom && noto)
        pntfuzz = true;

    if (bond) {
        CSeq_bond&  bond    = loc->SetBond();
        CSeq_point& point_a = bond.SetA();

        point_a.SetPoint(from);
        point_a.SetId(*MakeAccSeqId(ibp->acnum, pp->seqtype, pp->accver, ibp->vernum));

        if (fuzzfrom)
            GetIntFuzzPtr(4, 2, 0, point_a.SetFuzz());

        if (from != to) {
            CSeq_point& point_b = bond.SetB();
            point_b.SetPoint(to);
            point_b.SetId(*MakeAccSeqId(ibp->acnum, pp->seqtype, pp->accver, ibp->vernum));

            if (fuzzto)
                GetIntFuzzPtr(4, 1, 0, point_b.SetFuzz());
        }
    } else if (from != to && ! pntfuzz) {
        CSeq_interval& interval = loc->SetInt();
        interval.SetFrom(from);
        interval.SetTo(to);
        interval.SetId(*MakeAccSeqId(ibp->acnum, pp->seqtype, pp->accver, ibp->vernum));

        if (fuzzfrom)
            GetIntFuzzPtr(4, 2, 0, interval.SetFuzz_from()); /* lim, lt, no-min */

        if (nofrom)
            GetIntFuzzPtr(2, to - 1, 0, interval.SetFuzz_from()); /* range, max, min */

        if (noto)
            GetIntFuzzPtr(2, to, from + 1, interval.SetFuzz_to()); /* range, max, min */

        if (fuzzto)
            GetIntFuzzPtr(4, 1, 0, interval.SetFuzz_to()); /* lim, gt, no-min */
    } else {
        CSeq_point& point = loc->SetPnt();
        point.SetPoint(from);
        point.SetId(*MakeAccSeqId(ibp->acnum, pp->seqtype, pp->accver, ibp->vernum));

        if (pntfuzz) {
            GetIntFuzzPtr(2, to, from, point.SetFuzz()); /* range, max, min */
        } else if (fuzzfrom) {
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
static void DelTheStr(string& sourcestr, const string& targetstr)
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

    if (!spfip)
        return false;

    if (MatchArrayISubString(ParFlat_SPFeatNoExp, spfip->descrip) != -1)
        return true;

    indx = MatchArrayISubString(ParFlat_SPFeatNoExpW, spfip->descrip);
    if (indx < 0)
        return false;

    DelTheStr(spfip->descrip, ParFlat_SPFeatNoExpW[indx]);
    if (len > 0 && spfip->descrip[len-1] != '.')
    {
        StringCombine(spfip->descrip, ".", nullptr);
    }

    FtaErrPost(SEV_WARNING, ERR_FEATURE_OldNonExp,
              "Old Non-experimental feature description, {}",
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
static Int2 GetSPSitesMod(string& retstr)
{
    Int2 ret = ParFlatSPSitesModB;

    for (Int2 i = ParFlatSPSitesModB; i <= ParFlatSPSitesModE; i++) {
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

    return (ret);
}

/**********************************************************
 *
 *   Int2 SpFeatKeyNameValid(keystr):
 *
 *                                              10-18-93
 *
 **********************************************************/
Int2 SpFeatKeyNameValid(const Char* keystr)
{
    Int2 i;

    for (i = 0; ParFlat_SPFeat[i].inkey; i++)
        if (NStr::EqualNocase(ParFlat_SPFeat[i].inkey, keystr))
            break;

    if (! ParFlat_SPFeat[i].inkey)
        return (-1);
    return (i);
}

/**********************************************************/
CRef<CSeq_feat> SpProcFeatBlk(ParserPtr pp, FeatBlkPtr fbp, const CSeq_id& seqid)
{
    string descrip;
    Uint1  type;
    Int2   indx;
    bool   err = false;

    descrip.assign(CpTheQualValue(fbp->quals, "note"));

    if (NStr::EqualNocase(fbp->key, "VARSPLIC")) {
        FtaErrPost(SEV_WARNING, ERR_FEATURE_ObsoleteFeature, "Obsolete UniProt feature \"VARSPLIC\" found. Replaced with \"VAR_SEQ\".");
        fbp->key = "VAR_SEQ";
    }

    if (NStr::EqualNocase(fbp->key, "NON_STD")) {
        if (NStr::EqualNocase(descrip, "Selenocysteine.")) {
            fbp->key = "SE_CYS";
            descrip.clear();
        } else
            fbp->key = "MOD_RES";
    }

    CRef<CSeq_feat> feat(new CSeq_feat);
    indx = fbp->spindex;
    type = ParFlat_SPFeat[indx].type;
    if (type == ParFlatSPSites) {
        if (indx == ParFlatSPSitesModB && ! descrip.empty())
            indx = GetSPSitesMod(descrip);

        feat->SetData().SetSite(static_cast<CSeqFeatData::ESite>(ParFlat_SPFeat[indx].keyint));
    } else if (type == ParFlatSPBonds) {
        feat->SetData().SetBond(static_cast<CSeqFeatData::EBond>(ParFlat_SPFeat[indx].keyint));
    } else if (type == ParFlatSPRegions) {
        feat->SetData().SetRegion(ParFlat_SPFeat[indx].keystring);
    } else if (type == ParFlatSPImports) {
        feat->SetData().SetImp().SetKey(ParFlat_SPFeat[indx].keystring);
        feat->SetData().SetImp().SetDescr("uncertain amino acids");
    } else {
        if (type != ParFlatSPInitMet && type != ParFlatSPNonTer &&
            type != ParFlatSPNonCons) {
            FtaErrPost(SEV_WARNING, ERR_FEATURE_Dropped, "Swiss-Prot feature \"{}\" with unknown type dropped.", fbp->key);
        }
        feat->Reset();
        return (null);
    }

    if (fbp->location) {
        string& loc = *fbp->location;
        // strip spaces
        size_t j = 0;
        for (char c : loc)
            if (c != ' ')
                loc[j++] = c;
        loc.resize(j);
        pp->buf = fbp->key + " : " + *fbp->location;
        GetSeqLocation(*feat, *fbp->location, seqid, &err, pp, fbp->key);
        pp->buf.reset();
    }
    if (err) {
        if (! pp->debug) {
            FtaErrPost(SEV_ERROR, ERR_FEATURE_Dropped, "{}|{}| range check detects problems", fbp->key, *fbp->location);
            if (! descrip.empty())
                descrip.clear();
            feat->Reset();
            return (null);
        }
        FtaErrPost(SEV_WARNING, ERR_LOCATION_FailedCheck, "{}|{}| range check detects problems", fbp->key, *fbp->location);
    }

    if (SeqLocHaveFuzz(feat->GetLocation()))
        feat->SetPartial(true);

    if (! descrip.empty())
        feat->SetComment(descrip);

    return (feat);
}

/**********************************************************
 *
 *   static void SPFeatGeneral(pp, spfip, initmet):
 *
 *                                              10-18-93
 *
 **********************************************************/
static void SPFeatGeneral(ParserPtr pp, SPFeatInputList& spfil, bool initmet, CSeq_annot::C_Data::TFtable& feats)
{
    Int2 indx;
    bool signal;
    bool bond;
    /* bsv : 03/04/2020 : no Seq-feat.exp-ev setting anymore
    bool           noexp;
*/
    Uint1 type;

    for (auto& temp : spfil) {
        FtaInstallPrefix(PREFIX_FEATURE, temp.key, temp.from);

        if (NStr::EqualNocase("VARSPLIC", temp.key)) {
            FtaErrPost(SEV_WARNING, ERR_FEATURE_ObsoleteFeature, "Obsolete UniProt feature \"VARSPLIC\" found. Replaced with \"VAR_SEQ\".");
            temp.key = "VAR_SEQ";
        }

        if (NStr::EqualNocase(temp.key, "NON_STD")) {
            if (NStr::EqualNocase(temp.descrip, "Selenocysteine.")) {
                temp.key = "SE_CYS";
                temp.descrip.clear();
            } else
                temp.key = "MOD_RES";
        }

        indx = SpFeatKeyNameValid(temp.key.c_str());
        if (indx == -1) {
            FtaErrPost(SEV_WARNING, ERR_FEATURE_UnknownFeatKey, "dropping");
            FtaDeletePrefix(PREFIX_FEATURE);
            continue;
        }

        signal = false;
        bond   = false;

        /* bsv : 03/04/2020 : no Seq-feat.exp-ev setting anymore
        noexp = SPFeatNoExp(pp, temp);
*/

        CRef<CSeq_feat> feat(new CSeq_feat);

        type = ParFlat_SPFeat[indx].type;
        if (type == ParFlatSPSites) {
            if (indx == ParFlatSPSitesModB)
                indx = GetSPSitesMod(temp.descrip);

            feat->SetData().SetSite(static_cast<CSeqFeatData::ESite>(ParFlat_SPFeat[indx].keyint));
        } else if (type == ParFlatSPBonds) {
            feat->SetData().SetBond(static_cast<CSeqFeatData::EBond>(ParFlat_SPFeat[indx].keyint));
            bond = true;
        } else if (type == ParFlatSPRegions) {
            feat->SetData().SetRegion(ParFlat_SPFeat[indx].keystring);
            if (feat->GetData().GetRegion() == "Signal")
                signal = true;
        } else if (type == ParFlatSPImports) {
            feat->SetData().SetImp().SetKey(ParFlat_SPFeat[indx].keystring);
            feat->SetData().SetImp().SetDescr("uncertain amino acids");
        } else {
            if (type != ParFlatSPInitMet && type != ParFlatSPNonTer &&
                type != ParFlatSPNonCons) {
                FtaErrPost(SEV_WARNING, ERR_FEATURE_Dropped, "Swiss-Prot feature \"{}\" with unknown type dropped.", temp.key);
            }
            FtaDeletePrefix(PREFIX_FEATURE);
            continue;
        }

        /* bsv : 03/04/2020 : no Seq-feat.exp-ev setting anymore
        if(noexp)
            feat->SetExp_ev(CSeq_feat::eExp_ev_not_experimental);
        else
            feat->SetExp_ev(CSeq_feat::eExp_ev_experimental);
*/


        CRef<CSeq_loc> loc = GetSPSeqLoc(pp, temp, bond, initmet, signal);
        if (loc.NotEmpty())
            feat->SetLocation(*loc);

        if (SeqLocHaveFuzz(*loc))
            feat->SetPartial(true);

        if (! temp.descrip.empty())
            feat->SetComment(NStr::Sanitize(temp.descrip));

        feats.push_back(feat);

        FtaDeletePrefix(PREFIX_FEATURE);
    }
}

/**********************************************************/
static void DelParenthesis(string& str)
{
    auto p = str.cbegin();
    auto q = str.cend();
    while (p < q && (*p == ' ' || *p == '\t'))
        ++p;
    if (p < q) {
        --q;
        while (p < q && (*q == ' ' || *q == '\t'))
            --q;
        ++q;
    }

    auto pp = p;
    auto qq = q;
    while (pp < qq && *pp == '(')
        ++pp;
    while (pp < qq && *(qq - 1) == ')')
        --qq;

    Int2 count = 0, left = 0, right = 0;
    for (auto r = pp; r < qq; r++) {
        if (*r == '(')
            left++;
        else if (*r == ')') {
            right++;
            count = left - right;
        }
    }
    for (; count < 0 && pp > p; pp--)
        count++;
    count = 0;
    for (auto r = qq; r > pp;) {
        --r;
        if (*r == '(')
            count--;
        else if (*r == ')')
            count++;
    }
    for (; count < 0 && qq < q; qq++)
        count++;

    // str = string(pp, qq);
    if (qq < str.end())
        str.erase(qq, str.end());
    if (pp > str.begin())
        str.erase(str.begin(), pp);
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
static void CkGeneNameSP(string& gname)
{
    DelParenthesis(gname);
    for (char c : gname)
        if (! (isalnum(c) || c == '_' || c == '-' || c == '.' ||
               c == '\'' || c == '`' || c == '/' || c == '(' || c == ')')) {
            FtaErrPost(SEV_WARNING, ERR_GENENAME_IllegalGeneName, "gene_name contains unusual characters, {}, in SWISS-PROT", gname);
            break;
        }
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
static void ParseGeneNameSP(string_view str, CSeq_feat& feat)
{
    Int2  count = 0;

    CGene_ref& gene = feat.SetData().SetGene();

    for (auto p = str.begin(), e = str.end(); p < e;) {
        while (p < e && *p == ' ')
            p++;
        auto q = p;
        while (p < e && *p != ' ')
            p++;

        string_view tok(q, p);
        if (p < e)
            p++;
        if (tok == "AND"sv || tok == "OR"sv)
            continue;

        string gname(tok);
        CkGeneNameSP(gname);
        if (count == 0) {
            count++;
            gene.SetLocus(gname);
        } else {
            gene.SetSyn().push_back(gname);
        }
    }
}

/**********************************************************
 *
 *   static CRef<CSeq_loc> GetSeqLocIntSP(seqlen, acnum,
 *                                   accver, vernum):
 *
 *                                              10-18-93
 *
 **********************************************************/
static CRef<CSeq_loc> GetSeqLocIntSP(size_t seqlen, char* acnum, bool accver, Int2 vernum)
{
    CRef<CSeq_loc> loc(new CSeq_loc);
    CSeq_interval& interval = loc->SetInt();

    interval.SetFrom(0);
    interval.SetTo(static_cast<TSeqPos>(seqlen) - 1);
    interval.SetId(*MakeAccSeqId(acnum, CSeq_id::e_Swissprot, accver, vernum));

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
static void GetOneGeneRef(ParserPtr pp, CSeq_annot::C_Data::TFtable& feats, string_view bptr, size_t seqlen)
{
    if (! pp || pp->entrylist.empty())
        return;

    IndexblkPtr ibp = pp->entrylist[pp->curindx];
    if (! ibp)
        return;

    string str(bptr);
    for (char& c : str)
        if (c == '\t')
            c = ' ';

    CleanTailNonAlphaChar(str);

    CRef<CSeq_feat> feat(new CSeq_feat);
    ParseGeneNameSP(str, *feat);
    feat->SetLocation(*GetSeqLocIntSP(seqlen, ibp->acnum, pp->accver, ibp->vernum));

    feats.push_back(feat);
}

/**********************************************************/
static void SPFreeGenRefTokens(char* name, char* syns, char* ltags, char* orfs)
{
    if (name)
        MemFree(name);
    if (syns)
        MemFree(syns);
    if (ltags)
        MemFree(ltags);
    if (orfs)
        MemFree(orfs);
}

/**********************************************************/
static void SPParseGeneRefTag(char* str, CGene_ref& gene, bool set_locus_tag)
{
    char* p;
    char* q;

    if (! str)
        return;

    for (p = str; p && *p != '\0'; p = q) {
        while (*p == ' ' || *p == ',')
            p++;
        q = StringChr(p, ',');
        if (q)
            *q++ = '\0';
        if (q == p)
            continue;
        if (set_locus_tag && ! gene.IsSetLocus_tag()) {
            gene.SetLocus_tag(p);
            continue;
        }

        gene.SetSyn().push_back(p);
    }
}

/**********************************************************/
static void SPGetOneGeneRefNew(ParserPtr pp, CSeq_annot::C_Data::TFtable& feats, size_t seqlen, char* name, char* syns, char* ltags, char* orfs)
{
    IndexblkPtr ibp;

    if (! pp || pp->entrylist.empty() ||
        (! name && ! syns && ! ltags && ! orfs))
        return;

    ibp = pp->entrylist[pp->curindx];
    if (! ibp)
        return;

    CRef<CSeq_feat> feat(new CSeq_feat);
    CGene_ref&      gene = feat->SetData().SetGene();

    if (name)
        gene.SetLocus(name);


    SPParseGeneRefTag(syns, gene, false);
    SPParseGeneRefTag(ltags, gene, true);
    SPParseGeneRefTag(orfs, gene, true);

    feat->SetLocation(*GetSeqLocIntSP(seqlen, ibp->acnum, pp->accver, ibp->vernum));

    feats.push_back(feat);
}

/**********************************************************/
static void SPGetGeneRefsNew(ParserPtr pp, CSeq_annot::C_Data::TFtable& feats, string_view bptr, size_t seqlen)
{
    IndexblkPtr ibp;

    char* name;
    char* syns;
    char* ltags;
    char* orfs;
    char* str;
    char* p;
    char* q;
    char* r;

    if (! pp || pp->entrylist.empty() || bptr.empty())
        return;

    ibp = pp->entrylist[pp->curindx];
    if (! ibp)
        return;

    str = StringSave(bptr);

    name  = nullptr;
    syns  = nullptr;
    ltags = nullptr;
    orfs  = nullptr;
    for (p = str; p && *p != '\0'; p = q) {
        while (*p == ' ' || *p == ';')
            p++;
        for (r = p;; r = q + 1) {
            q = StringChr(r, ';');
            if (! q || q[1] == ' ' || q[1] == '\n' || q[1] == '\0')
                break;
        }
        if (q)
            *q++ = '\0';
        if (StringEquNI(p, "Name=", 5)) {
            if (name) {
                FtaErrPost(SEV_REJECT, ERR_FORMAT_ExcessGeneFields, "Field \"Name=\" occurs multiple times within a GN line. Entry dropped.");
                ibp->drop = true;
                break;
            }
            p += 5;
            if (p != q)
                name = StringSave(p);
        } else if (StringEquNI(p, "Synonyms=", 9)) {
            if (syns) {
                FtaErrPost(SEV_REJECT, ERR_FORMAT_ExcessGeneFields, "Field \"Synonyms=\" occurs multiple times within a GN line. Entry dropped.");
                ibp->drop = true;
                break;
            }
            p += 9;
            if (p != q)
                syns = StringSave(p);
        } else if (StringEquNI(p, "OrderedLocusNames=", 18)) {
            if (ltags) {
                FtaErrPost(SEV_REJECT, ERR_FORMAT_ExcessGeneFields, "Field \"OrderedLocusNames=\" occurs multiple times within a GN line. Entry dropped.");
                ibp->drop = true;
                break;
            }
            p += 18;
            if (p != q)
                ltags = StringSave(p);
        } else if (StringEquNI(p, "ORFNames=", 9)) {
            if (orfs) {
                FtaErrPost(SEV_REJECT, ERR_FORMAT_ExcessGeneFields, "Field \"ORFNames=\" occurs multiple times within a GN line. Entry dropped.");
                ibp->drop = true;
                break;
            }
            p += 9;
            if (p != q)
                orfs = StringSave(p);
        } else if (StringEquNI(p, "and ", 4)) {
            if (q)
                *--q = ';';
            q = p + 4;

            if (! name && ! syns && ! ltags && ! orfs)
                continue;

            if (! name && syns) {
                FtaErrPost(SEV_ERROR, ERR_FORMAT_MissingGeneName, "Encountered a gene with synonyms \"{}\" that lacks a gene symbol.", syns);
            }

            SPGetOneGeneRefNew(pp, feats, seqlen, name, syns, ltags, orfs);
            SPFreeGenRefTokens(name, syns, ltags, orfs);
            name  = nullptr;
            syns  = nullptr;
            ltags = nullptr;
            orfs  = nullptr;
        } else {
            FtaErrPost(SEV_REJECT, ERR_FORMAT_UnknownGeneField, "Field \"{}\" is not a legal field for the GN linetype. Entry dropped.", p);
            ibp->drop = true;
            break;
        }
    }

    MemFree(str);

    if (! name && ! syns && ! ltags && ! orfs)
        return;

    if (ibp->drop) {
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
static Int4 GetSeqLen(const DataBlk& entry)
{
    EntryBlkPtr    ebp    = entry.GetEntryData();
    const CBioseq& bioseq = ebp->seq_entry->GetSeq();
    return bioseq.GetLength();
}

/**********************************************************
 *
 *   static void SPFeatGeneRef(pp, hsfp, entry):
 *
 *      sfp->mpData: gene (Gene-ref).
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
static void SPFeatGeneRef(ParserPtr pp, CSeq_annot::C_Data::TFtable& feats, const DataBlk& entry)
{
    char* offset;
    size_t len = 0;
    if (! SrchNodeType(entry, ParFlatSP_GN, &len, &offset))
        return;

    string str = GetBlkDataReplaceNewLine(string_view(offset, len), ParFlat_COL_DATA_SP);
    StripECO(str);
    if (str.empty())
        return;

    len = GetSeqLen(entry);
    if (! StringIStr(str.c_str(), "Name=") &&
        ! StringIStr(str.c_str(), "Synonyms=") &&
        ! StringIStr(str.c_str(), "OrderedLocusNames=") &&
        ! StringIStr(str.c_str(), "ORFNames="))
        GetOneGeneRef(pp, feats, str, len);
    else
        SPGetGeneRefsNew(pp, feats, str, len);
}

/**********************************************************/
static void SPValidateEcnum(string& ecnum)
{
    char* p;
    char* q;
    char* buf;
    Int4  count;

    buf = StringSave(ecnum);
    for (count = 0, q = buf;; q = p) {
        p = q;
        count++;
        if (*p == '-') {
            p++;
            if (*p != '.')
                break;
            p++;
            continue;
        }
        if (*p == 'n') {
            p++;
            if (*p == '.' || *p == '\0') {
                count = 0;
                break;
            }
        }
        while (*p >= '0' && *p <= '9')
            p++;
        if (*q == 'n' && (*p == '.' || *p == '\0')) {
            fta_StringCpy(q + 1, p);
            p = q + 1;
        }
        if (p == q) {
            count = 0;
            break;
        }
        if (*p != '.')
            break;
        p++;
    }

    if (count != 4 || *p != '\0') {
        FtaErrPost(SEV_ERROR, ERR_FORMAT_InvalidECNumber, "Invalid EC number provided in SwissProt DE line: \"{}\". Preserve it anyway.", ecnum);
    } else
        ecnum = buf;
    MemFree(buf);
}

/**********************************************************/
static void SPCollectProtNames(const SPDEFieldList& sfl, SPDEFieldList::const_iterator sfp, CProt_ref& prot, Int4 tag)
{
    for (; sfp != sfl.cend(); ++sfp) {
        if (sfp->tag == SPDE_RECNAME || sfp->tag == SPDE_ALTNAME ||
            sfp->tag == SPDE_SUBNAME || sfp->tag == SPDE_FLAGS)
            break;
        if (sfp->tag != tag)
            continue;

        prot.SetName().push_back(string(sfp->start, sfp->end));
    }
}

/**********************************************************/
static void SPValidateDefinition(const SPDEFieldList& sfl, bool* drop, bool is_trembl)
{
    Int4 rcount = 0;
    Int4 scount = 0;
    Int4 fcount = 0;

    for (auto tsfp = sfl.cbegin(); tsfp != sfl.cend(); ++tsfp) {
        if (tsfp->tag == SPDE_RECNAME)
            rcount++;
        else if (tsfp->tag == SPDE_SUBNAME)
            scount++;
    }

    for (auto tsfp = sfl.cbegin(); tsfp != sfl.cend(); ++tsfp) {
        if (tsfp->tag != SPDE_RECNAME)
            continue;
        for (++tsfp; tsfp != sfl.cend(); ++tsfp) {
            if (tsfp->tag == SPDE_RECNAME || tsfp->tag == SPDE_ALTNAME ||
                tsfp->tag == SPDE_SUBNAME || tsfp->tag == SPDE_FLAGS)
                break;
            if (tsfp->tag == SPDE_FULL)
                fcount++;
        }
        if (tsfp == sfl.cend())
            break;
    }

    if (rcount > 1) {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_MultipleRecName, "This UniProt record has multiple RecName protein-name categories, but only one is allowed. Entry dropped.");
        *drop = true;
    } else if (rcount == 0 && ! is_trembl) {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_MissingRecName, "This UniProt/Swiss-Prot record lacks required RecName protein-name categorie. Entry dropped.");
        *drop = true;
    }

    if (scount > 0 && ! is_trembl) {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_SwissProtHasSubName, "This UniProt/Swiss-Prot record includes a SubName protein-name category, which should be used only for UniProt/TrEMBL. Entry dropped.");
        *drop = true;
    }

    if (fcount == 0 && rcount > 0) {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_MissingFullRecName, "This UniProt record lacks a Full name in the RecName protein-name category.");
        *drop = true;
    }
}

/**********************************************************/
static void SPParseDefinition(char* str, const CBioseq::TId& ids, IndexblkPtr ibp, CProt_ref& prot)
{
    SPDEFieldList sfl;
    bool is_trembl;
    const char* p;

    if (! str || (! NStr::StartsWith(str, "RecName: "sv, NStr::eNocase) &&
                  ! NStr::StartsWith(str, "AltName: "sv, NStr::eNocase) &&
                  ! NStr::StartsWith(str, "SubName: "sv, NStr::eNocase)))
        return;

    is_trembl = false;

    for (const auto& id : ids) {
        if (! id->IsSwissprot())
            continue;

        if (id->GetSwissprot().IsSetRelease() &&
            NStr::EqualNocase(id->GetSwissprot().GetRelease(), "unreviewed"sv))
            is_trembl = true;
    }

    sfl.emplace_front(0, nullptr);
    auto tsfp = sfl.begin();

    // Int4 count = 0;
    for (p = str; *p != '\0';) {
        while (*p == ' ')
            p++;
        auto q = p;
        while (*p != '\0' && *p != ' ')
            p++;
        string_view sv(q, p);
        const CharIntLen* cilp;
        for (cilp = spde_tags; cilp->num != 0; cilp++)
            if (NStr::StartsWith(sv, cilp->str, NStr::eNocase))
                break;

        if (cilp->num == 0)
            continue;

        if (tsfp->tag != 0) {
            if (q == tsfp->start)
                tsfp->end = q;
            else {
                const char* r;
                for (r = q - 1; *r == ' ' || *r == ';';)
                    r--;
                tsfp->end = r + 1;
            }
        }

        if (cilp->num == SPDE_INCLUDES || cilp->num == SPDE_CONTAINS)
            break;

        // count++;
        const char* r;
        for (r = q + cilp->str.size(); *r == ' ';)
            r++;
        tsfp = sfl.emplace_after(tsfp, cilp->num, r);
    }

    if (*p == '\0')
        tsfp->end = p;

    sfl.pop_front();

    SPValidateDefinition(sfl, &ibp->drop, is_trembl);

    for (auto tsfp = sfl.cbegin(); tsfp != sfl.cend(); ++tsfp)
        if (tsfp->tag == SPDE_RECNAME)
            SPCollectProtNames(sfl, next(tsfp), prot, SPDE_FULL);
    for (auto tsfp = sfl.cbegin(); tsfp != sfl.cend(); ++tsfp)
        if (tsfp->tag == SPDE_RECNAME)
            SPCollectProtNames(sfl, next(tsfp), prot, SPDE_SHORT);

    for (auto tsfp = sfl.cbegin(); tsfp != sfl.cend(); ++tsfp)
        if (tsfp->tag == SPDE_ALTNAME)
            SPCollectProtNames(sfl, next(tsfp), prot, SPDE_FULL);
    for (auto tsfp = sfl.cbegin(); tsfp != sfl.cend(); ++tsfp)
        if (tsfp->tag == SPDE_ALTNAME)
            SPCollectProtNames(sfl, next(tsfp), prot, SPDE_SHORT);

    for (auto tsfp = sfl.cbegin(); tsfp != sfl.cend(); ++tsfp)
        if (tsfp->tag == SPDE_SUBNAME)
            SPCollectProtNames(sfl, next(tsfp), prot, SPDE_FULL);
    for (auto tsfp = sfl.cbegin(); tsfp != sfl.cend(); ++tsfp)
        if (tsfp->tag == SPDE_SUBNAME)
            SPCollectProtNames(sfl, next(tsfp), prot, SPDE_SHORT);
}

/**********************************************************/
static void SPGetPEValue(const DataBlk& entry, CSeq_feat& feat)
{
    char* offset;
    char* buf;
    char* p;
    char* q;

    size_t len = 0;
    if (! SrchNodeType(entry, ParFlatSP_PE, &len, &offset))
        return;

    buf = StringSave(string_view(offset, len - 1));

    for (q = buf + 2; *q == ' ';)
        q++;
    p = StringChr(q, ':');
    if (p)
        for (p++; *p == ' ';)
            p++;
    else
        p = q;

    q = StringRChr(p, ';');
    if (! q)
        q = StringChr(p, '\n');
    if (q)
        *q = '\0';

    if (MatchArrayIString(PE_values, p) < 0)
        FtaErrPost(SEV_ERROR, ERR_SPROT_PELine, "Unrecognized value is encountered in PE (Protein Existence) line: \"{}\".", p);

    CRef<CGb_qual> qual(new CGb_qual);
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
static void SPFeatProtRef(ParserPtr pp, CSeq_annot::C_Data::TFtable& feats, const DataBlk& entry, SPFeatBlnPtr spfbp)
{
    IndexblkPtr ibp;

    char* offset;

    char*  str;
    string str1;

    char* ptr;

    const char* tag;
    Char        symb;
    Int4        shift;

    EntryBlkPtr ebp;

    ebp = entry.GetEntryData();

    CSeq_entry& seq_entry = *ebp->seq_entry;
    CBioseq&    bioseq    = seq_entry.SetSeq();

    size_t len = 0;
    if (! SrchNodeType(entry, ParFlatSP_DE, &len, &offset))
        return;

    CRef<CSeq_feat> feat(new CSeq_feat);
    CProt_ref&      prot = feat->SetData().SetProt();

    string str_ = GetBlkDataReplaceNewLine(string_view(offset, len), ParFlat_COL_DATA_SP);
    StripECO(str_);
    while (! str_.empty()) {
        char c = str_.back();
        if (c == '.' || c == ';' || c == ',')
            str_.pop_back();
        else
            break;
    }

    ShrinkSpaces(str_);
    str = StringSave(str_);

    ibp = pp->entrylist[pp->curindx];

    if (NStr::StartsWith(str_, "Contains: "sv, NStr::eNocase) ||
        NStr::StartsWith(str_, "Includes: "sv, NStr::eNocase)) {
        FtaErrPost(SEV_REJECT, ERR_FORMAT_NoProteinNameCategory, "DE lines do not have a non-Includes/non-Contains RecName, AltName or SubName protein name category. Entry dropped.");
        ibp->drop = true;
    }

    if (NStr::StartsWith(str_, "RecName: "sv, NStr::eNocase) ||
        NStr::StartsWith(str_, "AltName: "sv, NStr::eNocase) ||
        NStr::StartsWith(str_, "SubName: "sv, NStr::eNocase)) {
        tag   = "; EC=";
        symb  = ';';
        shift = 5;
        SPParseDefinition(str, bioseq.GetId(), ibp, prot);
    } else {
        tag   = "(EC";
        symb  = ')';
        shift = 3;
    }

    while ((ptr = StringStr(str, tag))) {
        len = StringLen(str);
        str1.assign(str, ptr);

        ptr += shift;
        while (*ptr == ' ')
            ptr++;

        char* bptr;
        for (bptr = ptr; *ptr != '\0' && *ptr != ' ' && *ptr != symb;)
            ptr++;
        if (ptr > bptr) {
            string ecnum(bptr, ptr);
            SPValidateEcnum(ecnum);

            if (! ecnum.empty())
                prot.SetEc().push_back(ecnum);
        } else {
            FtaErrPost(SEV_WARNING, ERR_FORMAT_ECNumberNotPresent, "Empty EC number provided in SwissProt DE line.");
        }

        if (symb == ')') {
            while (*ptr != '\0' && (*ptr == ' ' || *ptr == symb))
                ptr++;
            if (StringLen(ptr) <= 1)
                NStr::TruncateSpacesInPlace(str1, NStr::eTrunc_End);
        }

        str1 += ptr;

        MemFree(str);
        str = StringSave(str1);
    }

    if (symb == ')') {
        while ((ptr = StringStr(str, " (")) ||
               (ptr = StringStr(str, " /"))) {
            str1.assign(str, ptr);
            NStr::TruncateSpacesInPlace(str1, NStr::eTrunc_End);

            MemFree(str);
            str = StringSave(str1);
        }
    }

    if (! prot.IsSetName())
        prot.SetName().push_back(str);

    MemFree(str);

    feat->SetLocation(*GetSeqLocIntSP(GetSeqLen(entry), ibp->acnum, pp->accver, ibp->vernum));

    if (spfbp->nonter) {
        feat->SetPartial(true);

        if (spfbp->noleft)
            GetIntFuzzPtr(4, 2, 0, feat->SetLocation().SetInt().SetFuzz_from()); /* lim, lt, no-min */
        if (spfbp->noright)
            GetIntFuzzPtr(4, 1, 0, feat->SetLocation().SetInt().SetFuzz_to()); /* lim, gt, no-min */
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
static SPSegLocList GetSPSegLocInfo(CBioseq& bioseq, const SPFeatInputList& spfil, SPFeatBlnPtr spfbp)
{
    SPSegLocList spsll;
    SPSegLocList::iterator curspslp;

    if (spfil.empty())
        return spsll;

    /* get location range
     */
    for (const auto& spfip : spfil) {
        if (spfip.key != "NON_CONS")
            continue;

        if (spsll.empty()) {
            spsll.emplace_front(0);
            curspslp = spsll.begin();
        }

        const char* p = spfip.from.c_str();
        if (*p == '<' || *p == '>' || *p == '?')
            p++;
        Int4 from = fta_atoi(p);
        curspslp->len = from - curspslp->from;
        curspslp = spsll.emplace_after(curspslp, from);
    }

    for (auto& descr : bioseq.SetDescr().Set()) {
        if (! descr->IsMolinfo())
            continue;

        if (spfbp->noleft && spfbp->noright)
            descr->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_no_ends);
        else if (spfbp->noleft)
            descr->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_no_left);
        else if (spfbp->noright)
            descr->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_no_right);
    }

    if (! spsll.empty())
        curspslp->len = bioseq.GetLength() - curspslp->from;

    return spsll;
}

/**********************************************************
 *
 *   static void CkInitMetSP(pp, spfip, sep, spfbp):
 *
 *                                              11-1-93
 *
 **********************************************************/
static void CkInitMetSP(ParserPtr pp, const SPFeatInputList& spfil, CSeq_entry& seq_entry, SPFeatBlnPtr spfbp)
{
    Int2 count = 0;
    Int4 from  = 0;
    Int4 to;

    auto spfip = spfil.cbegin();
    auto temp  = spfil.cend();

    for (; spfip != spfil.cend(); ++spfip) {
        if (spfip->key != "INIT_MET")
            continue;

        if (count > 0)
            break;

        count++;
        const char* p = spfip->from.c_str();
        if (*p == '<' || *p == '>' || *p == '?')
            p++;
        from = fta_atoi(p);
        p    = spfip->to.c_str();
        if (*p == '<' || *p == '>' || *p == '?')
            p++;
        to = fta_atoi(p);

        if ((from != 0 || to != 0) && (from != 1 || to != 1))
            break;
        temp = spfip;
    }

    if (count == 0)
        return;

    if (spfip != spfil.cend()) {
        FtaErrPost(SEV_ERROR, ERR_FEATURE_Invalid_INIT_MET, "Either incorrect or more than one INIT_MET feature provided.");
        return;
    }

    if (! temp->descrip.empty()) {
        FtaErrPost(SEV_WARNING, ERR_FEATURE_ExpectEmptyComment, "{}:{}-{} has description: {}", temp->key, from, to, temp->descrip);
    }


    CBioseq& bioseq = seq_entry.SetSeq();

    CSeq_data& data     = bioseq.SetInst().SetSeq_data();
    string&    sequence = data.SetIupacaa().Set();

    if (from == 0) {
        spfbp->initmet = true;

        /* insert "M" in the front
         */
        sequence.insert(sequence.begin(), 'M');
        bioseq.SetInst().SetLength(static_cast<TSeqPos>(sequence.size()));
    } else if (sequence.empty() || sequence[0] != 'M')
        FtaErrPost(SEV_ERROR, ERR_FEATURE_MissingInitMet, "The required Init Met is missing from the sequence.");
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
static void CkNonTerSP(ParserPtr pp, const SPFeatInputList& spfil, CSeq_entry& seq_entry, SPFeatBlnPtr spfbp)
{
    Int4 from;
    Int4 ctr;
    bool segm;

    CMolInfo* mol_info = nullptr;
    CBioseq&  bioseq   = seq_entry.SetSeq();

    ctr = 0;
    for (auto& descr : bioseq.SetDescr().Set()) {
        if (! descr->IsMolinfo())
            continue;

        mol_info = &(descr->SetMolinfo());
        break;
    }

    segm = false;
    for (const auto& temp : spfil) {
        if (temp.key == "NON_CONS") {
            segm = true;
            continue;
        }

        if (temp.key != "NON_TER")
            continue;

        from = NStr::StringToInt(temp.from);
        if (from != NStr::StringToInt(temp.to)) {
            FtaErrPost(SEV_WARNING, ERR_FEATURE_UnEqualEndPoint, "NON_TER has unequal endpoints");
            continue;
        }

        if (from == 1) {
            spfbp->nonter = true;
            spfbp->noleft = true;
        } else if (from == (Int4)pp->entrylist[pp->curindx]->bases) {
            spfbp->nonter  = true;
            spfbp->noright = true;
        } else {
            FtaErrPost(SEV_WARNING, ERR_FEATURE_NotSeqEndPoint, "NON_TER is not at a sequence endpoint.");
        }
    }

    if (! mol_info)
        return;

    if (segm && mol_info->GetCompleteness() != 2) {
        mol_info->SetCompleteness(CMolInfo::eCompleteness_partial);
        FtaErrPost(SEV_WARNING, ERR_FEATURE_NoFragment, "Found NON_CONS in FT line but no FRAGMENT in DE line.");
    } else if (spfbp->nonter && mol_info->GetCompleteness() != CMolInfo::eCompleteness_partial) {
        mol_info->SetCompleteness(CMolInfo::eCompleteness_partial);
        FtaErrPost(SEV_WARNING, ERR_FEATURE_NoFragment, "Found NON_TER in FT line but no FRAGMENT in DE line.");
    } else if (! spfbp->nonter && mol_info->GetCompleteness() == CMolInfo::eCompleteness_partial && ! segm) {
        FtaErrPost(SEV_WARNING, ERR_FEATURE_PartialNoNonTerNonCons, "Entry is partial but has no NON_TER or NON_CONS features.");
    } else if (mol_info->GetCompleteness() != 2) {
        if (bioseq.GetInst().IsSetSeq_data()) {
            const CSeq_data& data     = bioseq.GetInst().GetSeq_data();
            const string&    sequence = data.GetIupacaa().Get();

            for (string::const_iterator value = sequence.begin(); value != sequence.end(); ++value) {
                if (*value != 'X') {
                    ctr = 0; /* reset counter */
                    continue;
                }

                ctr++;
                if (ctr == 5) {
                    mol_info->SetCompleteness(CMolInfo::eCompleteness_partial);
                    break;
                }
            }
        }
    }
}

/**********************************************************/
static void SeqToDeltaSP(CBioseq& bioseq, const SPSegLocList& spsll)
{
    if (spsll.empty() || ! bioseq.GetInst().IsSetSeq_data())
        return;

    CSeq_ext::TDelta& deltas      = bioseq.SetInst().SetExt().SetDelta();
    const string&     bioseq_data = bioseq.GetInst().GetSeq_data().GetIupacaa().Get();

    for (auto spslp = spsll.begin(); spslp != spsll.end(); ++spslp) {
        CRef<CDelta_seq> delta(new CDelta_seq);
        if (! deltas.Set().empty()) {
            delta->SetLiteral().SetLength(0);
            delta->SetLiteral().SetFuzz().SetLim();
            deltas.Set().push_back(delta);

            delta.Reset(new CDelta_seq);
        }

        delta->SetLiteral().SetLength(spslp->len);


        string data_str = bioseq_data.substr(spslp->from, spslp->len);

        delta->SetLiteral().SetSeq_data().SetIupacaa().Set(data_str);
        deltas.Set().push_back(delta);
    }

    if (deltas.Set().size() > 1) {
        bioseq.SetInst().SetRepr(CSeq_inst::eRepr_delta);
        bioseq.SetInst().ResetSeq_data();
    } else
        bioseq.SetInst().SetExt().Reset();
}

/**********************************************************
 *
 *   static void GetSPAnnot(pp, entry, protconv):
 *
 *                                              10-15-93
 *
 **********************************************************/
static void GetSPAnnot(ParserPtr pp, const DataBlk& entry, unsigned char* protconv)
{
    EntryBlkPtr  ebp;
    SPFeatBlnPtr spfbp;
    SPSegLocList spsll; /* segment location, data from NON_CONS */

    ebp                   = entry.GetEntryData();
    CSeq_entry& seq_entry = *ebp->seq_entry;

    spfbp = new SPFeatBln;
    auto spfil = ParseSPFeat(entry, pp->entrylist[pp->curindx]->bases);

    CSeq_annot::C_Data::TFtable feats;

    if (! spfil.empty()) {
        CkNonTerSP(pp, spfil, seq_entry, spfbp);
        CkInitMetSP(pp, spfil, seq_entry, spfbp);
        SPFeatGeneral(pp, spfil, spfbp->initmet, feats);
    }

    SPFeatGeneRef(pp, feats, entry);        /* GN line */
    SPFeatProtRef(pp, feats, entry, spfbp); /* DE line */

    CBioseq& bioseq = seq_entry.SetSeq();

    spsll = GetSPSegLocInfo(bioseq, spfil, spfbp); /* checking NON_CONS key */
    if (! spsll.empty())
        SeqToDeltaSP(bioseq, spsll);

    if (! feats.empty()) {
        CRef<CSeq_annot> annot(new CSeq_annot);
        annot->SetData().SetFtable().swap(feats);
        bioseq.SetAnnot().push_back(annot);
    }

    spsll.clear();
    delete spfbp;
}

/**********************************************************/
static void SpPrepareEntry(ParserPtr pp, const DataBlk& entry, unsigned char* protconv)
{
    Int2        curkw;
    char*       ptr;
    char*       eptr;
    EntryBlkPtr ebp;

    ebp  = entry.GetEntryData();
    ptr  = entry.mBuf.ptr;
    eptr = ptr + entry.mBuf.len;
    for (curkw = ParFlatSP_ID; curkw != ParFlatSP_END;) {
        ptr = GetEmblBlock(ebp->chain, ptr, &curkw, pp->format, eptr);
    }
    GetSprotSubBlock(pp, entry);

    if (pp->entrylist[pp->curindx]->bases == 0) {
        SpAddToIndexBlk(entry, pp->entrylist[pp->curindx]);
    }

    CRef<CBioseq> bioseq = CreateEntryBioseq(pp);
    ebp->seq_entry.Reset(new CSeq_entry);
    ebp->seq_entry->SetSeq(*bioseq);
    GetScope().AddBioseq(*bioseq);

    GetSprotDescr(*bioseq, pp, entry);

    GetSPInst(pp, entry, protconv);
    GetSPAnnot(pp, entry, protconv);

    TEntryList entries;
    entries.push_back(ebp->seq_entry);
    fta_find_pub_explore(pp, entries);

    if (pp->citat) {
        StripSerialNumbers(entries);
    }
}


CRef<CSeq_entry> CSwissProt2Asn::xGetEntry()
{
    CRef<CSeq_entry> pResult;
    if (mParser.curindx >= mParser.indx) {
        return pResult;
    }

    IndexblkPtr ibp = mParser.entrylist[mParser.curindx];

    err_install(ibp, mParser.accver);

    if (! ibp->drop) {
        auto entry = LoadEntry(&mParser, ibp->offset, ibp->len);
        if (! entry) {
            FtaDeletePrefix(PREFIX_LOCUS | PREFIX_ACCESSION);
            NCBI_THROW(CException, eUnknown, "Unable to load entry");
        }

        SpPrepareEntry(&mParser, *entry, GetProtConvTable());

        if (! ibp->drop) {
            pResult = entry->GetEntryData()->seq_entry;
        }

        GetScope().ResetDataAndHistory();
    }
    if (! ibp->drop) {
        mTotals.Succeeded++;
        FtaErrPost(SEV_INFO, ERR_ENTRY_Parsed, "OK - entry \"{}|{}\" parsed successfully", ibp->locusname, ibp->acnum);
    } else {
        FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry \"{}|{}\" skipped", ibp->locusname, ibp->acnum);
    }
    mParser.curindx++;
    return pResult;
}


void CSwissProt2Asn::PostTotals()
{   
    FtaErrPost(SEV_INFO, ERR_ENTRY_ParsingComplete, 
            "Parsing completed, {} entr{} parsed", 
            mTotals.Succeeded, (mTotals.Succeeded == 1) ? "y" : "ies");
}

END_NCBI_SCOPE
