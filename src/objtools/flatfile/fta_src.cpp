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
 * File Name: fta_src.cpp
 *
 * Author: Sergey Bazhin
 *
 * File Description:
 *      Messes about source features.
 */

#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_annot_.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seq/Seq_descr.hpp>

#include "index.h"

#include <objtools/flatfile/flatdefn.h>
#include "ftanet.h"

#include "ftaerr.hpp"
#include "asci_blk.h"
#include "loadfeat.h"
#include "utilfeat.h"
#include "add.h"
#include "utilfun.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "fta_src.cpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

struct CharUInt1 {
    const char*       name;
    COrgMod::ESubtype num;
};

#define USE_CULTIVAR         00001
#define USE_ISOLATE          00002
#define USE_SEROTYPE         00004
#define USE_SEROVAR          00010
#define USE_SPECIMEN_VOUCHER 00020
#define USE_STRAIN           00040
#define USE_SUB_SPECIES      00100
#define USE_SUB_STRAIN       00200
#define USE_VARIETY          00400
#define USE_ECOTYPE          01000
#define USE_ALL              01777

#define BIOSOURCES_THRESHOLD 20

struct PcrPrimers {
    char*       fwd_name = nullptr;
    char*       fwd_seq  = nullptr;
    char*       rev_name = nullptr;
    char*       rev_seq  = nullptr;
    PcrPrimers* next     = nullptr;
};
using PcrPrimersPtr = PcrPrimers*;

struct SourceFeatBlk {
    char* name            = nullptr;
    char* strain          = nullptr;
    char* organelle       = nullptr;
    char* isolate         = nullptr;
    char* namstr          = nullptr;
    char* location        = nullptr;
    char* moltype         = nullptr;
    char* genomename      = nullptr;
    char* submitter_seqid = nullptr;

    TQualVector      quals;
    CRef<CBioSource> bio_src;
    CRef<COrgName>   orgname;

    bool full   = false;
    bool focus  = false;
    bool tg     = false;
    bool lookup = false;
    bool skip   = false;
    bool useit  = false;

    CBioSource::EGenome genome = CBioSource::eGenome_unknown;
    SourceFeatBlk*      next   = nullptr;
};
using SourceFeatBlkPtr = SourceFeatBlk*;

struct MinMax {
    const char* orgname = nullptr; /* Do not free! It's just a pointer */
    Int4        min     = 0;
    Int4        max     = 0;
    bool        skip    = false;
    MinMax*     next    = nullptr;
};
using MinMaxPtr = MinMax*;

static const char* ObsoleteSourceDbxrefTag[] = {
    "IFO",
    nullptr
};

// DENLR = DDBJ + EMBL + NCBI + LANL + RefSeq
static const char* DENLRSourceDbxrefTag[] = {
    "AFTOL",
    "ANTWEB",
    "ATCC",
    "ATCC(DNA)",
    "ATCC(IN HOST)",
    "BEI",
    "BOLD",
    "FBOL",
    "FUNGORUM",
    "GREENGENES",
    "GRIN",
    "HMP",
    "HOMD",
    "IKMC",
    "ISHAM-ITS",
    "JCM",
    "NBRC",
    "RBGE_GARDEN",
    "RBGE_HERBARIUM",
    "RZPD",
    "UNILIB",
    nullptr
};

// DE = DDBJ + EMBL
static const char* DESourceDbxrefTag[] = {
    "FANTOM_DB",
    "IMGT/HLA",
    "IMGT/LIGM",
    "MGD",
    "MGI",
    nullptr
};

// E = EMBL
static const char* ESourceDbxrefTag[] = {
    "UNITE",
    nullptr
};

// NLR = NCBI + LANL + RefSeq
static const char* NLRSourceDbxrefTag[] = {
    "FLYBASE",
    nullptr
};

static const char* exempt_quals[] = {
    "transposon",
    "insertion_seq",
    nullptr
};

static const char* special_orgs[] = {
    "synthetic construct",
    "artificial sequence",
    "eukaryotic synthetic construct",
    nullptr
};

static const char* unusual_toks[] = {
    "complement",
    nullptr
};

static const char* source_genomes[] = {
    "mitochondr",
    "chloroplast",
    "kinetoplas",
    "cyanelle",
    "plastid",
    "chromoplast",
    "macronuclear",
    "extrachrom",
    "plasmid",
    nullptr
};

static const char* SourceBadQuals[] = {
    "label",
    "usedin",
    "citation",
    nullptr
};

static const char* SourceSubSources[] = {
    "chromosome",           /*  1 = CSubSource::eSubtype_chromosome, etc. */
    "map",                  /*  2 */
    "clone",                /*  3 */
    "sub_clone",            /*  4 */
    "haplotype",            /*  5 */
    "genotype",             /*  6 */
    "sex",                  /*  7 */
    "cell_line",            /*  8 */
    "cell_type",            /*  9 */
    "tissue_type",          /* 10 */
    "clone_lib",            /* 11 */
    "dev_stage",            /* 12 */
    "frequency",            /* 13 */
    "germline",             /* 14 */
    "rearranged",           /* 15 */
    "lab_host",             /* 16 */
    "pop_variant",          /* 17 */
    "tissue_lib",           /* 18 */
    "plasmid",              /* 19 */
    "transposon",           /* 20 */
    "insertion_seq",        /* 21 */
    "plastid",              /* 22 */
    "",                     /* 23 */
    "segment",              /* 24 */
    "",                     /* 25 */
    "transgenic",           /* 26 */
    "environmental_sample", /* 27 */
    "isolation_source",     /* 28 */
    "lat_lon",              /* 29 */
    "collection_date",      /* 30 */
    "collected_by",         /* 31 */
    "identified_by",        /* 32 */
    "",                     /* 33 */
    "",                     /* 34 */
    "",                     /* 35 */
    "",                     /* 36 */
    "metagenomic",          /* 37 */
    "mating_type",          /* 38 */
    nullptr
};

// clang-format off
static const CharUInt1 SourceOrgMods[] = {
    { "strain",             COrgMod::eSubtype_strain },
    { "sub_strain",         COrgMod::eSubtype_substrain },
    { "variety",            COrgMod::eSubtype_variety },
    { "serotype",           COrgMod::eSubtype_serotype },
    { "serovar",            COrgMod::eSubtype_serovar },
    { "cultivar",           COrgMod::eSubtype_cultivar },
    { "isolate",            COrgMod::eSubtype_isolate },
    { "specific_host",      COrgMod::eSubtype_nat_host },
    { "host",               COrgMod::eSubtype_nat_host },
    { "sub_species",        COrgMod::eSubtype_sub_species },
    { "specimen_voucher",   COrgMod::eSubtype_specimen_voucher },
    { "ecotype",            COrgMod::eSubtype_ecotype },
    { "culture_collection", COrgMod::eSubtype_culture_collection },
    { "bio_material",       COrgMod::eSubtype_bio_material },
    { "metagenome_source",  COrgMod::eSubtype_metagenome_source },
    { "type_material",      COrgMod::eSubtype_type_material },
};
// clang-format on

static const char* GenomicSourceFeatQual[] = {
    "unknown", // CBioSource::eGenome_unknown, etc.
    "unknown", // ?
    "chloroplast",
    "chromoplast",
    "kinetoplast",
    "mitochondrion",
    "plastid",
    "macronuclear",
    "extrachrom",
    "plasmid",
    "transposon",
    "insertion-seq",
    "cyanelle",
    "proviral",
    "virion",
    "nucleomorph",
    "apicoplast",
    "leucoplast",
    "proplastid",    /* 18 */
    "",              /* 19 */
    "",              /* 20 */
    "",              /* 21 */
    "chromatophore", /* 22 */
    nullptr
};

static const char* OrganelleFirstToken[] = {
    "chromatophore",
    "hydrogenosome",
    "mitochondrion",
    "nucleomorph",
    "plastid",
    nullptr
};

static const char *NullTermValues[] = {
    "missing",
    "not applicable",
    "not collected",
    "not provided",
    "restricted access",
    "missing: control sample",
    "missing: sample group",
    "missing: synthetic construct",
    "missing: lab stock",
    "missing: third party data",
    "missing: data agreement established pre-2023",
    "missing: endangered species",
    "missing: human-identifiable",
    nullptr
};

/**********************************************************/
static SourceFeatBlkPtr SourceFeatBlkNew(void)
{
    return new SourceFeatBlk;
}

/**********************************************************/
static void SourceFeatBlkFree(SourceFeatBlkPtr sfbp)
{
    if (sfbp->name)
        MemFree(sfbp->name);
    if (sfbp->strain)
        MemFree(sfbp->strain);
    if (sfbp->organelle)
        MemFree(sfbp->organelle);
    if (sfbp->isolate)
        MemFree(sfbp->isolate);
    if (sfbp->namstr)
        MemFree(sfbp->namstr);
    if (sfbp->location)
        MemFree(sfbp->location);
    if (sfbp->moltype)
        MemFree(sfbp->moltype);
    if (sfbp->genomename)
        MemFree(sfbp->genomename);

    delete sfbp;
}

/**********************************************************/
static void SourceFeatBlkSetFree(SourceFeatBlkPtr sfbp)
{
    SourceFeatBlkPtr tsfbp;

    for (tsfbp = sfbp; tsfbp; tsfbp = sfbp) {
        sfbp = tsfbp->next;
        SourceFeatBlkFree(tsfbp);
    }
}

/**********************************************************/
static SourceFeatBlkPtr CollectSourceFeats(DataBlkCIter dbp, DataBlkCIter dbp_end, Int2 type)
{
    SourceFeatBlkPtr sfbp;
    SourceFeatBlkPtr tsfbp;

    sfbp  = SourceFeatBlkNew();
    tsfbp = sfbp;

    for (; dbp != dbp_end; ++dbp) {
        if (dbp->mType != type)
            continue;
        for (const auto& tdbp : dbp->GetSubBlocks()) {
            const FeatBlk* fbp = tdbp.GetFeatData();
            if (! fbp || fbp->key != "source")
                continue;
            tsfbp->next = SourceFeatBlkNew();
            tsfbp       = tsfbp->next;
            if (fbp->location_isset())
                tsfbp->location = StringSave(fbp->location_get());
            tsfbp->quals = fbp->quals;
        }
    }
    tsfbp = sfbp->next;
    delete sfbp;
    return (tsfbp);
}

/**********************************************************/
static void RemoveStringSpaces(char* line)
{
    char* p;
    char* q;

    if (! line || *line == '\0')
        return;

    for (p = line, q = line; *p != '\0'; p++)
        if (*p != ' ' && *p != '\t')
            *q++ = *p;
    *q = '\0';
}

/**********************************************************/
static void RemoveSourceFeatSpaces(SourceFeatBlkPtr sfbp)
{
    for (; sfbp; sfbp = sfbp->next) {
        RemoveStringSpaces(sfbp->location);
        for (auto& cur : sfbp->quals) {
            if (cur->IsSetQual()) {
                ShrinkSpaces(cur->SetQual());
            }
            if (cur->IsSetVal()) {
                ShrinkSpaces(cur->SetVal());
            }
        }
    }
}

/**********************************************************/
static void CheckForExemption(SourceFeatBlkPtr sfbp)
{
    const char** b;

    for (; sfbp; sfbp = sfbp->next) {
        for (const auto& cur : sfbp->quals) {
            for (b = exempt_quals; *b; b++) {
                if (cur->GetQual() == *b)
                    break;
            }
            if (*b) {
                sfbp->skip = true;
                break;
            }
        }
    }
}

/**********************************************************/
static void PopulateSubNames(string& namstr, const Char* name, const Char* value, COrgMod::ESubtype subtype, TOrgModList& mods)
{
    CRef<COrgMod> mod(new COrgMod);

    namstr.append(name);
    namstr.append(value);
    namstr.append(")");

    mod->SetSubtype(subtype);
    mod->SetSubname(value);

    mods.push_front(mod);
}

/**********************************************************/
static void CollectSubNames(SourceFeatBlkPtr sfbp, Int4 use_what, const Char* name, const Char* cultivar, const Char* isolate, const Char* serotype, const Char* serovar, const Char* specimen_voucher, const Char* strain, const Char* sub_species, const Char* sub_strain, const Char* variety, const Char* ecotype)
{
    if (! sfbp)
        return;

    if (sfbp->namstr)
        MemFree(sfbp->namstr);
    sfbp->namstr = nullptr;

    if (sfbp->orgname.NotEmpty())
        sfbp->orgname.Reset();

    if (! name)
        return;

    size_t i = 0;
    if ((use_what & USE_CULTIVAR) == USE_CULTIVAR && cultivar)
        i += (StringLen(cultivar) + StringLen("cultivar") + 5);
    if ((use_what & USE_ISOLATE) == USE_ISOLATE && isolate)
        i += (StringLen(isolate) + StringLen("isolate") + 5);
    if ((use_what & USE_SEROTYPE) == USE_SEROTYPE && serotype)
        i += (StringLen(serotype) + StringLen("serotype") + 5);
    if ((use_what & USE_SEROVAR) == USE_SEROVAR && serovar)
        i += (StringLen(serovar) + StringLen("serovar") + 5);
    if ((use_what & USE_SPECIMEN_VOUCHER) == USE_SPECIMEN_VOUCHER && specimen_voucher)
        i += (StringLen(specimen_voucher) + StringLen("specimen_voucher") + 5);
    if ((use_what & USE_STRAIN) == USE_STRAIN && strain)
        i += (StringLen(strain) + StringLen("strain") + 5);
    if ((use_what & USE_SUB_SPECIES) == USE_SUB_SPECIES && sub_species)
        i += (StringLen(sub_species) + StringLen("sub_species") + 5);
    if ((use_what & USE_SUB_STRAIN) == USE_SUB_STRAIN && sub_strain)
        i += (StringLen(sub_strain) + StringLen("sub_strain") + 5);
    if ((use_what & USE_VARIETY) == USE_VARIETY && variety)
        i += (StringLen(variety) + StringLen("variety") + 5);
    if ((use_what & USE_ECOTYPE) == USE_ECOTYPE && ecotype)
        i += (StringLen(ecotype) + StringLen("ecotype") + 5);

    if (i == 0) {
        sfbp->namstr = StringSave(name);
        return;
    }

    sfbp->orgname     = new COrgName;
    TOrgModList& mods = sfbp->orgname->SetMod();

    string s = name;
    s.reserve(s.size() + i);
    if ((use_what & USE_CULTIVAR) == USE_CULTIVAR && cultivar)
        PopulateSubNames(s, "  (cultivar ", cultivar, COrgMod::eSubtype_cultivar, mods);
    if ((use_what & USE_ISOLATE) == USE_ISOLATE && isolate)
        PopulateSubNames(s, "  (isolate ", isolate, COrgMod::eSubtype_isolate, mods);
    if ((use_what & USE_SEROTYPE) == USE_SEROTYPE && serotype)
        PopulateSubNames(s, "  (serotype ", serotype, COrgMod::eSubtype_serotype, mods);
    if ((use_what & USE_SEROVAR) == USE_SEROVAR && serovar)
        PopulateSubNames(s, "  (serovar ", serovar, COrgMod::eSubtype_serovar, mods);
    if ((use_what & USE_SPECIMEN_VOUCHER) == USE_SPECIMEN_VOUCHER && specimen_voucher)
        PopulateSubNames(s, "  (specimen_voucher ", specimen_voucher, COrgMod::eSubtype_specimen_voucher, mods);
    if ((use_what & USE_STRAIN) == USE_STRAIN && strain)
        PopulateSubNames(s, "  (strain ", strain, COrgMod::eSubtype_strain, mods);
    if ((use_what & USE_SUB_SPECIES) == USE_SUB_SPECIES && sub_species)
        PopulateSubNames(s, "  (sub_species ", sub_species, COrgMod::eSubtype_sub_species, mods);
    if ((use_what & USE_SUB_STRAIN) == USE_SUB_STRAIN && sub_strain)
        PopulateSubNames(s, "  (sub_strain ", sub_strain, COrgMod::eSubtype_substrain, mods);
    if ((use_what & USE_VARIETY) == USE_VARIETY && variety)
        PopulateSubNames(s, "  (variety ", variety, COrgMod::eSubtype_variety, mods);
    if ((use_what & USE_ECOTYPE) == USE_ECOTYPE && ecotype)
        PopulateSubNames(s, "  (ecotype ", ecotype, COrgMod::eSubtype_ecotype, mods);
    sfbp->namstr = StringSave(s);
}

/**********************************************************/
static bool SourceFeatStructFillIn(IndexblkPtr ibp, SourceFeatBlkPtr sfbp, Int4 use_what)
{
    const Char** b;

    const Char* name;
    const Char* cultivar;
    const Char* isolate;
    const Char* organelle;
    const Char* serotype;
    const Char* serovar;
    const Char* ecotype;
    const Char* specimen_voucher;
    const Char* strain;
    const Char* sub_species;
    const Char* sub_strain;
    const Char* variety;
    char*       genomename;
    Char*       p;
    char*       q;
    bool        ret;
    Int4        i;

    for (ret = true; sfbp; sfbp = sfbp->next) {
        name             = nullptr;
        cultivar         = nullptr;
        isolate          = nullptr;
        organelle        = nullptr;
        serotype         = nullptr;
        serovar          = nullptr;
        ecotype          = nullptr;
        specimen_voucher = nullptr;
        strain           = nullptr;
        sub_species      = nullptr;
        sub_strain       = nullptr;
        variety          = nullptr;
        genomename       = nullptr;

        for (auto& cur : sfbp->quals) {
            if (! cur->IsSetQual())
                continue;

            const string& qual_str = cur->GetQual();
            char*         val_ptr  = cur->IsSetVal() ? cur->SetVal().data() : nullptr;

            if (qual_str == "db_xref") {
                q = StringChr(val_ptr, ':');
                if (! q || q[1] == '\0')
                    continue;
                *q = '\0';
                if (NStr::CompareNocase(val_ptr, "taxon") == 0)
                    if (ibp->taxid <= ZERO_TAX_ID)
                        ibp->taxid = TAX_ID_FROM(int, atoi(q + 1));
                *q = ':';
                continue;
            }
            if (qual_str == "focus") {
                sfbp->focus = true;
                continue;
            }
            if (qual_str == "transgenic") {
                sfbp->tg = true;
                continue;
            }
            if (qual_str == "cultivar") {
                cultivar = val_ptr;
                continue;
            }
            if (qual_str == "isolate") {
                if (! isolate)
                    isolate = val_ptr;
                continue;
            }
            if (qual_str == "mol_type") {
                if (sfbp->moltype)
                    ret = false;
                else if (val_ptr)
                    sfbp->moltype = StringSave(val_ptr);
                continue;
            }
            if (qual_str == "organelle") {
                if (! organelle)
                    organelle = val_ptr;
                continue;
            }
            if (qual_str == "serotype") {
                serotype = val_ptr;
                continue;
            }
            if (qual_str == "serovar") {
                serovar = val_ptr;
                continue;
            }
            if (qual_str == "ecotype") {
                ecotype = val_ptr;
                continue;
            }
            if (qual_str == "specimen_voucher") {
                specimen_voucher = val_ptr;
                continue;
            }
            if (qual_str == "strain") {
                if (! strain)
                    strain = val_ptr;
                continue;
            }
            if (qual_str == "sub_species") {
                sub_species = val_ptr;
                continue;
            }
            if (qual_str == "sub_strain") {
                sub_strain = val_ptr;
                continue;
            }
            if (qual_str == "variety") {
                variety = val_ptr;
                continue;
            }
            if (qual_str == "submitter_seqid") {
                if (sfbp->submitter_seqid) {
                    MemFree(sfbp->submitter_seqid);
                    sfbp->submitter_seqid = StringSave("");
                } else
                    sfbp->submitter_seqid = StringSave(val_ptr);
                if (ibp->submitter_seqid.empty())
                    ibp->submitter_seqid = StringSave(val_ptr);
                continue;
            }

            if (qual_str != "organism" ||
                ! val_ptr || val_ptr[0] == '\0')
                continue;

            if (ibp->organism.empty())
                ibp->organism = val_ptr;

            p = StringChr(val_ptr, ' ');

            string str_to_find;
            if (p)
                str_to_find.assign(val_ptr, p);
            else
                str_to_find.assign(val_ptr);

            for (i = 0, b = source_genomes; *b; b++, i++)
                if (StringEquNI(str_to_find.c_str(), *b, StringLen(*b)))
                    break;
            if (*b && i != 8) {
                if (genomename)
                    MemFree(genomename);
                genomename = StringSave(str_to_find);
            }

            if (p)
                ++p;

            if (! *b)
                p = val_ptr;
            else {
                if (i == 0)
                    sfbp->genome = CBioSource::eGenome_mitochondrion;
                else if (i == 1)
                    sfbp->genome = CBioSource::eGenome_chloroplast;
                else if (i == 2)
                    sfbp->genome = CBioSource::eGenome_kinetoplast;
                else if (i == 3)
                    sfbp->genome = CBioSource::eGenome_cyanelle;
                else if (i == 4)
                    sfbp->genome = CBioSource::eGenome_plastid;
                else if (i == 5)
                    sfbp->genome = CBioSource::eGenome_chromoplast;
                else if (i == 6)
                    sfbp->genome = CBioSource::eGenome_macronuclear;
                else if (i == 7)
                    sfbp->genome = CBioSource::eGenome_extrachrom;
                else if (i == 8) {
                    p            = val_ptr;
                    sfbp->genome = CBioSource::eGenome_plasmid;
                }
            }
            name = p;
        }

        if (sfbp->name)
            MemFree(sfbp->name);
        sfbp->name = name ? StringSave(name) : nullptr;

        if (sfbp->genomename)
            MemFree(sfbp->genomename);
        sfbp->genomename = genomename;

        if (strain && ! sfbp->strain)
            sfbp->strain = StringSave(strain);
        if (isolate && ! sfbp->isolate)
            sfbp->isolate = StringSave(isolate);
        if (organelle && ! sfbp->organelle)
            sfbp->organelle = StringSave(organelle);

        CollectSubNames(sfbp, use_what, name, cultivar, isolate, serotype, serovar, specimen_voucher, strain, sub_species, sub_strain, variety, ecotype);
    }
    return (ret);
}

/**********************************************************/
static char* CheckSourceFeatFocusAndTransposon(SourceFeatBlkPtr sfbp)
{
    for (; sfbp; sfbp = sfbp->next) {
        if (sfbp->focus && sfbp->skip)
            break;
    }

    if (sfbp)
        return (sfbp->location);
    return nullptr;
}

/**********************************************************/
static char* CheckSourceFeatOrgs(SourceFeatBlkPtr sfbp, int* status)
{
    *status = 0;
    for (; sfbp; sfbp = sfbp->next) {
        /** if (sfbp->namstr) */
        if (sfbp->name)
            continue;

        *status = (sfbp->genome == CBioSource::eGenome_unknown) ? 1 : 2;
        break;
    }
    if (sfbp)
        return (sfbp->location);
    return nullptr;
}

/**********************************************************/
static bool CheckSourceFeatLocFuzz(SourceFeatBlkPtr sfbp)
{
    const char** b;
    char*        p;
    char*        q;
    Int4         count;
    bool         partial;
    bool         invalid;
    bool         ret;

    ret = true;
    for (; sfbp; sfbp = sfbp->next) {
        if (! sfbp->location || sfbp->location[0] == '\0')
            break;
        if (sfbp->skip)
            continue;

        for (const auto& cur : sfbp->quals) {
            if (cur->GetQual() != "partial")
                continue;

            FtaErrPost(SEV_ERROR, ERR_SOURCE_PartialQualifier, "Source feature location has /partial qualifier. Qualifier has been ignored: \"{}\".", sfbp->location ? sfbp->location : "?empty?");
            break;
        }

        for (b = unusual_toks; *b; b++) {
            p = StringStr(sfbp->location, *b);
            if (! p)
                continue;
            q = p + StringLen(*b);
            if (p > sfbp->location)
                p--;
            if ((p == sfbp->location || *p == '(' || *p == ')' ||
                 *p == ':' || *p == ',' || *p == '.') &&
                (*q == '\0' || *q == '(' || *q == ')' || *q == ',' ||
                 *q == ':' || *q == '.')) {
                FtaErrPost(SEV_ERROR, ERR_SOURCE_UnusualLocation, "Source feature has an unusual location: \"{}\".", sfbp->location ? sfbp->location : "?empty?");
                break;
            }
        }

        partial = false;
        invalid = false;
        for (count = 0, p = sfbp->location; *p != '\0'; p++) {
            if (*p == '^')
                invalid = true;
            else if (*p == '>' || *p == '<')
                partial = true;
            else if (*p == '(')
                count++;
            else if (*p == ')')
                count--;
            else if (*p == '.' && p[1] == '.')
                p++;
            else if (*p == '.' && p[1] != '.') {
                for (q = p + 1; *q >= '0' && *q <= '9';)
                    q++;
                if (q == p || *q != ':')
                    invalid = true;
            }
        }
        if (partial) {
            FtaErrPost(SEV_ERROR, ERR_SOURCE_PartialLocation, "Source feature location is partial; partiality flags have been ignored: \"{}\".", sfbp->location ? sfbp->location : "?empty?");
        }
        if (invalid || count != 0) {
            FtaErrPost(SEV_REJECT, ERR_SOURCE_InvalidLocation, "Invalid location for source feature at \"{}\". Entry dropped.", sfbp->location ? sfbp->location : "?empty?");
            ret = false;
        }
    }
    return (ret);
}

/**********************************************************/
static char* CheckSourceFeatLocAccs(SourceFeatBlkPtr sfbp, char* acc)
{
    char* p;
    char* q;
    char* r;
    Int4  i;

    for (; sfbp; sfbp = sfbp->next) {
        if (! sfbp->location || sfbp->location[0] == '\0')
            continue;
        for (p = sfbp->location + 1; *p != '\0'; p++) {
            if (*p != ':')
                continue;
            for (r = nullptr, q = p - 1;; q--) {
                if (q == sfbp->location) {
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
            if (r)
                *r = '\0';
            else
                *p = '\0';
            i = NStr::CompareNocase(q, acc);
            if (r)
                *r = '.';
            else
                *p = ':';
            if (i != 0)
                break;
        }
        if (*p != '\0')
            break;
    }
    if (! sfbp)
        return nullptr;
    return (sfbp->location);
}

/**********************************************************/
static void MinMaxFree(MinMaxPtr mmp)
{
    MinMaxPtr tmmp;

    for (; mmp; mmp = tmmp) {
        tmmp = mmp->next;
        delete mmp;
    }
}

/**********************************************************/
bool fta_if_special_org(const Char* name)
{
    const char** b;

    if (! name || *name == '\0')
        return false;

    for (b = special_orgs; *b; b++)
        if (NStr::CompareNocase(*b, name) == 0)
            break;
    if (*b || StringIStr(name, "vector"))
        return true;
    return false;
}

/**********************************************************/
static Int4 CheckSourceFeatCoverage(SourceFeatBlkPtr sfbp, MinMaxPtr mmp, size_t len)
{
    SourceFeatBlkPtr tsfbp;
    MinMaxPtr        tmmp;
    MinMaxPtr        mmpnext;
    char*            p;
    char*            q;
    char*            r;
    char*            loc;
    Int4             count;
    Int4             min;
    Int4             max;
    Int4             i;
    Int4             tgs;
    Int4             sporg;

    loc = nullptr;
    for (tsfbp = sfbp; tsfbp; tsfbp = tsfbp->next) {
        if (! tsfbp->location || tsfbp->location[0] == '\0' ||
            ! tsfbp->name || tsfbp->name[0] == '\0')
            continue;
        if (loc)
            MemFree(loc);
        loc = StringSave(tsfbp->location);
        for (p = loc; *p != '\0'; p++)
            if (*p == ',' || *p == '(' || *p == ')' || *p == ':' ||
                *p == ';' || *p == '^')
                *p = ' ';
        for (p = loc, q = loc; *p != '\0';) {
            if (*p == '>' || *p == '<') {
                p++;
                continue;
            }
            *q++ = *p;
            if (*p == ' ')
                while (*p == ' ')
                    p++;
            else
                p++;
        }
        if (q > loc && *(q - 1) == ' ')
            q--;
        *q = '\0';

        q = (*loc == ' ') ? (loc + 1) : loc;
        for (p = q;;) {
            min = 0;
            max = 0;
            p   = StringChr(p, ' ');
            if (p)
                *p++ = '\0';
            for (r = q; *r >= '0' && *r <= '9';)
                r++;
            if (*r == '\0') {
                i = atoi(q);
                if (i > 0) {
                    min = i;
                    max = i;
                }
            } else if (*r == '.' && r[1] == '.') {
                *r++ = '\0';
                min  = atoi(q);
                if (min > 0) {
                    for (q = ++r; *r >= '0' && *r <= '9';)
                        r++;
                    if (*r == '\0')
                        max = atoi(q);
                }
            }
            if (min > 0 && max > 0) {
                if (min == 1 && (size_t)max == len)
                    tsfbp->full = true;
                for (tmmp = mmp;; tmmp = tmmp->next) {
                    if (min < tmmp->min) {
                        mmpnext             = tmmp->next;
                        tmmp->next          = new MinMax;
                        tmmp->next->orgname = tmmp->orgname;
                        tmmp->next->min     = tmmp->min;
                        tmmp->next->max     = tmmp->max;
                        tmmp->next->skip    = tmmp->skip;
                        tmmp->next->next    = mmpnext;
                        tmmp->orgname       = tsfbp->name;
                        tmmp->min           = min;
                        tmmp->max           = max;
                        tmmp->skip          = tsfbp->skip;
                        break;
                    }
                    if (! tmmp->next) {
                        tmmp->next          = new MinMax;
                        tmmp->next->orgname = tsfbp->name;
                        tmmp->next->min     = min;
                        tmmp->next->max     = max;
                        tmmp->next->skip    = tsfbp->skip;
                        break;
                    }
                }
            }

            if (! p)
                break;
            q = p;
        }
    }
    if (loc)
        MemFree(loc);

    mmp = mmp->next;
    if (! mmp || mmp->min != 1)
        return (1);

    for (max = mmp->max; mmp; mmp = mmp->next)
        if (mmp->max > max && mmp->min <= max + 1)
            max = mmp->max;

    if ((size_t)max < len)
        return (1);

    tgs   = 0;
    count = 0;
    sporg = 0;
    for (tsfbp = sfbp, i = 0; tsfbp; tsfbp = tsfbp->next, i++) {
        if (! tsfbp->full)
            continue;

        if (fta_if_special_org(tsfbp->name))
            sporg++;

        count++;
        if (tsfbp->tg)
            tgs++;
    }

    if (count < 2)
        return (0);
    if (count > 2 || i > count || (tgs != 1 && sporg != 1))
        return (2);
    return (0);
}

/**********************************************************/
static char* CheckWholeSourcesVersusFocused(SourceFeatBlkPtr sfbp)
{
    char* p     = nullptr;
    bool  whole = false;

    for (; sfbp; sfbp = sfbp->next) {
        if (sfbp->full)
            whole = true;
        else if (sfbp->focus)
            p = sfbp->location;
    }

    if (whole)
        return (p);
    return nullptr;
}

/**********************************************************/
static bool CheckSYNTGNDivision(SourceFeatBlkPtr sfbp, char* div)
{
    char* p;
    bool  got;
    bool  ret;
    Int4  syntgndiv;
    Char  ch;

    syntgndiv = 0;
    if (div && *div != '\0') {
        if (StringEqu(div, "SYN"))
            syntgndiv = 1;
        else if (StringEqu(div, "TGN"))
            syntgndiv = 2;
    }

    for (ret = true, got = false; sfbp; sfbp = sfbp->next) {
        if (! sfbp->tg)
            continue;

        if (syntgndiv == 0) {
            p = sfbp->location;
            if (p && StringLen(p) > 50) {
                ch    = p[50];
                p[50] = '\0';
            } else
                ch = '\0';
            FtaErrPost(SEV_REJECT, ERR_DIVISION_TransgenicNotSYN_TGN, "Source feature located at \"{}\" has a /transgenic qualifier, but this record is not in the SYN or TGN division.", p ? p : "unknown");
            if (ch != '\0')
                p[50] = ch;
            ret = false;
        }

        if (sfbp->full)
            got = true;
    }

    if (syntgndiv == 2 && ! got)
        FtaErrPost(SEV_ERROR, ERR_DIVISION_TGNnotTransgenic, "This record uses the TGN division code, but there is no full-length /transgenic source feature.");
    return (ret);
}

/**********************************************************/
static Int4 CheckTransgenicSourceFeats(SourceFeatBlkPtr sfbp)
{
    SourceFeatBlkPtr tsfbp;
    char*            taxname;
    bool             same;
    bool             tgfull;

    if (! sfbp)
        return (0);

    Int4 ret   = 0;
    bool tgs   = false;
    bool focus = false;
    for (tsfbp = sfbp; tsfbp; tsfbp = tsfbp->next) {
        if (tsfbp->tg) {
            if (! tsfbp->full)
                ret = 1; /* /transgenic on not full-length */
            else if (tgs)
                ret = 3; /* multiple /transgenics */
            if (ret != 0)
                break;
            tgs = true;
        }
        if (tsfbp->focus)
            focus = true;
        if (tgs && focus) {
            ret = 2; /* /focus and /transgenic */
            break;
        }
    }

    if (ret != 0)
        return (ret);

    same    = true;
    tgfull  = false;
    taxname = nullptr;
    for (tsfbp = sfbp; tsfbp; tsfbp = tsfbp->next) {
        if (tsfbp->skip)
            continue;
        if (! taxname)
            taxname = tsfbp->name;
        else if (same && ! fta_strings_same(taxname, tsfbp->name))
            same = false;
        if (tsfbp->tg && tsfbp->full)
            tgfull = true;
        if (tsfbp->focus)
            focus = true;
    }

    if (same == false && tgfull == false && focus == false)
        return (4);

    if (! sfbp->next || ! tgs)
        return (0);

    for (tsfbp = sfbp->next; tsfbp; tsfbp = tsfbp->next)
        if (fta_strings_same(sfbp->name, tsfbp->name) == false ||
            fta_strings_same(sfbp->strain, tsfbp->strain) == false ||
            fta_strings_same(sfbp->isolate, tsfbp->isolate) == false ||
            fta_strings_same(sfbp->organelle, tsfbp->organelle) == false)
            break;

    if (! tsfbp)
        return (5); /* all source features have the same
                                           /organism, /strain, /isolate and
                                           /organelle qualifiers */
    return (0);
}

/**********************************************************/
static Int4 CheckFocusInOrgs(SourceFeatBlkPtr sfbp, size_t len, int* status)
{
    SourceFeatBlkPtr tsfbp;
    const char**     b;
    char*            name;
    string           pat;
    Int4             count;
    bool             same;

    count = 0;
    name  = nullptr;
    same  = true;
    for (tsfbp = sfbp; tsfbp; tsfbp = tsfbp->next) {
        if (! tsfbp->name)
            continue;
        if (tsfbp->focus)
            count++;
        if (! name)
            name = tsfbp->name;
        else if (NStr::CompareNocase(name, tsfbp->name) != 0)
            same = false;
    }
    if (same && count > 0)
        (*status)++;

    name = nullptr;
    for (tsfbp = sfbp; tsfbp; tsfbp = tsfbp->next) {
        if (! tsfbp->focus || ! tsfbp->name)
            continue;
        if (! name)
            name = tsfbp->name;
        else if (NStr::CompareNocase(name, tsfbp->name) != 0)
            break;
    }
    if (tsfbp)
        return (2);

    if (same || count != 0)
        return (0);

    name = nullptr;
    pat  = "1.." + to_string(len);
    for (tsfbp = sfbp; tsfbp; tsfbp = tsfbp->next) {
        if (! tsfbp->name || ! tsfbp->location || tsfbp->skip)
            continue;

        for (b = special_orgs; *b; b++) {
            if (NStr::CompareNocase(*b, tsfbp->name) == 0 &&
                StringEqu(tsfbp->location, pat.c_str()))
                break;
        }
        if (*b)
            continue;

        if (! name)
            /** name = tsfbp->namstr;*/
            name = tsfbp->name;
        /** else if(NStr::CompareNocase(name, tsfbp->namstr) != 0)*/
        else if (NStr::CompareNocase(name, tsfbp->name) != 0)
            break;
    }

    if (! tsfbp)
        return (0);

    for (tsfbp = sfbp; tsfbp; tsfbp = tsfbp->next) {
        if (tsfbp->full && tsfbp->tg && ! tsfbp->skip)
            break;
    }

    if (tsfbp)
        return (0);
    return (3);
}

/**********************************************************/
static bool IfSpecialFeat(MinMaxPtr mmp, size_t len)
{
    if ((mmp->min == 1 && (size_t)mmp->max == len) || mmp->skip)
        return true;
    return false;
}

/**********************************************************/
static char* CheckSourceOverlap(MinMaxPtr mmp, size_t len)
{
    MinMaxPtr tmmp;
    char*     res;

    for (; mmp; mmp = mmp->next) {
        if (IfSpecialFeat(mmp, len))
            continue;
        for (tmmp = mmp->next; tmmp; tmmp = tmmp->next) {
            if (IfSpecialFeat(tmmp, len))
                continue;
            if (NStr::CompareNocase(mmp->orgname, tmmp->orgname) == 0)
                continue;
            if (tmmp->min <= mmp->max && tmmp->max >= mmp->min)
                break;
        }
        if (tmmp)
            break;
    }
    if (! mmp)
        return nullptr;

    stringstream ss;
    ss << "\"" << mmp->orgname << "\" at " << mmp->min << ".." << mmp->max
       << " vs \"" << tmmp->orgname << "\" at " << tmmp->min << ".." << tmmp->max;
    res = StringSave(ss.str());
    return res;
}

/**********************************************************/
static char* CheckForUnusualFullLengthOrgs(SourceFeatBlkPtr sfbp)
{
    SourceFeatBlkPtr tsfbp;
    const char**     b;

    if (! sfbp || ! sfbp->next)
        return nullptr;

    for (tsfbp = sfbp->next; tsfbp; tsfbp = tsfbp->next)
        if (NStr::CompareNocase(sfbp->name, tsfbp->name) != 0)
            break;

    if (! tsfbp)
        return nullptr;

    for (tsfbp = sfbp; tsfbp; tsfbp = tsfbp->next)
        if (tsfbp->full && tsfbp->tg)
            break;

    if (tsfbp)
        return nullptr;

    for (; sfbp; sfbp = sfbp->next) {
        if (! sfbp->full || sfbp->tg)
            continue;

        for (b = special_orgs; *b; b++)
            if (NStr::CompareNocase(*b, sfbp->name) == 0)
                break;

        if (*b)
            continue;

        if (! StringIStr(sfbp->name, "vector"))
            break;
    }
    if (! sfbp)
        return nullptr;
    return (sfbp->name);
}

/**********************************************************/
static void CreateRawBioSources(ParserPtr pp, SourceFeatBlkPtr sfbp, Int4 use_what)
{
    SourceFeatBlkPtr tsfbp;
    char*            namstr;
    const Char*      cultivar;
    const Char*      isolate;
    const Char*      serotype;
    const Char*      serovar;
    const Char*      ecotype;
    const Char*      specimen_voucher;
    const Char*      strain;
    const Char*      sub_species;
    const Char*      sub_strain;
    const Char*      variety;

    for (; sfbp; sfbp = sfbp->next) {
        if (sfbp->bio_src.NotEmpty())
            continue;

        namstr = StringSave(sfbp->namstr);
        CRef<COrg_ref> org_ref(new COrg_ref);
        org_ref->SetTaxname(sfbp->name);

        if (sfbp->orgname.NotEmpty()) {
            org_ref->SetOrgname(*sfbp->orgname);
        }

        CRef<COrg_ref> t_org_ref(new COrg_ref);
        t_org_ref->Assign(*org_ref);
        fta_fix_orgref(pp, *org_ref, &pp->entrylist[pp->curindx]->drop, sfbp->genomename);

        if (t_org_ref->Equals(*org_ref))
            sfbp->lookup = false;
        else {
            sfbp->lookup = true;
            MemFree(sfbp->name);
            sfbp->name = StringSave(org_ref->GetTaxname());

            sfbp->orgname.Reset();

            cultivar         = nullptr;
            isolate          = nullptr;
            serotype         = nullptr;
            serovar          = nullptr;
            ecotype          = nullptr;
            specimen_voucher = nullptr;
            strain           = nullptr;
            sub_species      = nullptr;
            sub_strain       = nullptr;
            variety          = nullptr;
            if (org_ref->IsSetOrgname() && org_ref->IsSetOrgMod()) {
                for (const auto& mod : org_ref->GetOrgname().GetMod()) {
                    switch (mod->GetSubtype()) {
                    case COrgMod::eSubtype_cultivar:
                        cultivar = mod->GetSubname().c_str();
                        break;
                    case COrgMod::eSubtype_isolate:
                        isolate = mod->GetSubname().c_str();
                        break;
                    case COrgMod::eSubtype_serotype:
                        serotype = mod->GetSubname().c_str();
                        break;
                    case COrgMod::eSubtype_serovar:
                        serovar = mod->GetSubname().c_str();
                        break;
                    case COrgMod::eSubtype_ecotype:
                        ecotype = mod->GetSubname().c_str();
                        break;
                    case COrgMod::eSubtype_specimen_voucher:
                        specimen_voucher = mod->GetSubname().c_str();
                        break;
                    case COrgMod::eSubtype_strain:
                        strain = mod->GetSubname().c_str();
                        break;
                    case COrgMod::eSubtype_sub_species:
                        sub_species = mod->GetSubname().c_str();
                        break;
                    case COrgMod::eSubtype_substrain:
                        sub_strain = mod->GetSubname().c_str();
                        break;
                    case COrgMod::eSubtype_variety:
                        variety = mod->GetSubname().c_str();
                        break;
                    }
                }
            }
            CollectSubNames(sfbp, use_what, sfbp->name, cultivar, isolate, serotype, serovar, specimen_voucher, strain, sub_species, sub_strain, variety, ecotype);
        }

        sfbp->bio_src.Reset(new CBioSource);
        sfbp->bio_src->SetOrg(*org_ref);

        for (tsfbp = sfbp->next; tsfbp; tsfbp = tsfbp->next) {
            if (tsfbp->bio_src.NotEmpty() || NStr::CompareNocase(namstr, tsfbp->namstr) != 0)
                continue;

            tsfbp->lookup = sfbp->lookup;

            tsfbp->bio_src.Reset(new CBioSource);
            tsfbp->bio_src->Assign(*sfbp->bio_src);

            if (! sfbp->lookup)
                continue;

            MemFree(tsfbp->name);
            tsfbp->name = StringSave(sfbp->name);

            MemFree(tsfbp->namstr);
            tsfbp->namstr = StringSave(sfbp->namstr);
        }
        MemFree(namstr);
    }
}

/**********************************************************/
static SourceFeatBlkPtr SourceFeatMoveOneUp(SourceFeatBlkPtr where,
                                            SourceFeatBlkPtr what)
{
    SourceFeatBlkPtr prev;
    SourceFeatBlkPtr tsfbp;

    if (what == where)
        return (where);

    prev = where;
    for (tsfbp = where->next; tsfbp; tsfbp = tsfbp->next) {
        if (tsfbp == what)
            break;
        prev = tsfbp;
    }
    if (! tsfbp)
        return (where);

    prev->next = what->next;
    what->next = where;
    return (what);
}

/**********************************************************/
static SourceFeatBlkPtr SourceFeatRemoveDups(SourceFeatBlkPtr sfbp)
{
    SourceFeatBlkPtr tsfbp;
    SourceFeatBlkPtr prev;
    SourceFeatBlkPtr next;

    for (prev = sfbp, tsfbp = sfbp->next; tsfbp; tsfbp = next) {
        next = tsfbp->next;
        if (! tsfbp->useit) {
            prev = tsfbp;
            continue;
        }

        bool different = false;
        for (const auto& cur : tsfbp->quals) {
            const string& cur_qual = cur->GetQual();
            if (cur_qual == "focus")
                continue;

            bool found = false;
            for (const auto& next : sfbp->quals) {
                const string& next_qual = next->GetQual();

                if (next_qual == "focus" || next_qual != cur_qual)
                    continue;

                if (! cur->IsSetVal() && ! next->IsSetVal()) {
                    found = true;
                    break;
                }

                if (cur->IsSetVal() && next->IsSetVal() &&
                    cur->GetVal() == next->GetVal()) {
                    found = true;
                    break;
                }
            }

            if (! found) /* Different, leave as is */
            {
                different = true;
                break;
            }
        }

        if (different) /* Different, leave as is */
        {
            prev = tsfbp;
            continue;
        }
        prev->next  = tsfbp->next;
        tsfbp->next = nullptr;
        SourceFeatBlkFree(tsfbp);
    }
    return (sfbp);
}

/**********************************************************/
static SourceFeatBlkPtr SourceFeatDerive(SourceFeatBlkPtr sfbp,
                                         SourceFeatBlkPtr res)
{
    SourceFeatBlkPtr tsfbp;

    if (! res)
        return (sfbp);

    tsfbp           = SourceFeatBlkNew();
    tsfbp->name     = res->name ? StringSave(res->name) : nullptr;
    tsfbp->namstr   = res->namstr ? StringSave(res->namstr) : nullptr;
    tsfbp->location = res->location ? StringSave(res->location) : nullptr;
    tsfbp->full     = res->full;
    tsfbp->focus    = res->focus;
    tsfbp->lookup   = res->lookup;
    tsfbp->genome   = res->genome;
    tsfbp->next     = nullptr;

    tsfbp->bio_src.Reset(new CBioSource);
    tsfbp->bio_src->Assign(*res->bio_src);

    tsfbp->orgname.Reset(new COrgName);
    if (res->orgname.NotEmpty())
        tsfbp->orgname->Assign(*res->orgname);

    tsfbp->quals = res->quals;
    tsfbp->next  = sfbp;
    sfbp         = tsfbp;

    for (TQualVector::iterator cur = sfbp->quals.begin(); cur != sfbp->quals.end();) {
        const string& cur_qual = (*cur)->GetQual();
        if (cur_qual == "focus") {
            ++cur;
            continue;
        }

        for (tsfbp = sfbp->next; tsfbp; tsfbp = tsfbp->next) {
            if (tsfbp == res || ! tsfbp->useit)
                continue;

            bool found = false;
            for (const auto& next : tsfbp->quals) {
                const string& next_qual = next->GetQual();

                if (next_qual == "focus" || next_qual != cur_qual)
                    continue;

                if (! (*cur)->IsSetVal() && ! next->IsSetVal()) {
                    found = true;
                    break;
                }

                if ((*cur)->IsSetVal() && next->IsSetVal() &&
                    (*cur)->GetVal() == next->GetVal()) {
                    found = true;
                    break;
                }
            }

            if (! found) /* Not found */
                break;
        }

        if (! tsfbp) /* Got the match */
        {
            ++cur;
            continue;
        }

        cur = sfbp->quals.erase(cur);
    }

    return (SourceFeatRemoveDups(sfbp));
}

/**********************************************************/
static SourceFeatBlkPtr PickTheDescrSource(SourceFeatBlkPtr sfbp)
{
    SourceFeatBlkPtr res;
    SourceFeatBlkPtr tsfbp;

    if (! sfbp->next) {
        if (! sfbp->full) {
            FtaErrPost(SEV_WARNING, ERR_SOURCE_SingleSourceTooShort, "Source feature does not span the entire length of the sequence.");
        }
        return (sfbp);
    }

    NCBI_UNUSED Int4 count_skip = 0;
    Int4 count_noskip = 0;
    bool same         = true;
    for (tsfbp = sfbp; tsfbp; tsfbp = tsfbp->next) {
        if (NStr::CompareNocase(tsfbp->name, sfbp->name) != 0) {
            same = false;
            break;
        }

        if (! tsfbp->skip) {
            res = tsfbp;
            count_noskip++;
        } else
            count_skip++;
    }

    if (same) {
        if (count_noskip == 1) {
            sfbp = SourceFeatMoveOneUp(sfbp, res);
            return (SourceFeatRemoveDups(sfbp));
        }
        for (res = nullptr, tsfbp = sfbp; tsfbp; tsfbp = tsfbp->next) {
            if (count_noskip != 0 && tsfbp->skip)
                continue;
            tsfbp->useit = true;
            if (! res)
                res = tsfbp;
        }
        return (SourceFeatDerive(sfbp, res));
    }

    for (tsfbp = sfbp; tsfbp; tsfbp = tsfbp->next) {
        if (tsfbp->tg)
            break;
    }
    if (tsfbp)
        return (SourceFeatMoveOneUp(sfbp, tsfbp));

    for (res = nullptr, tsfbp = sfbp; tsfbp; tsfbp = tsfbp->next) {
        if (! tsfbp->focus)
            continue;
        res = tsfbp;
        if (! tsfbp->skip)
            break;
    }

    if (res) {
        count_skip   = 0;
        count_noskip = 0;
        for (tsfbp = sfbp; tsfbp; tsfbp = tsfbp->next) {
            if (NStr::CompareNocase(res->name, tsfbp->name) != 0)
                continue;
            tsfbp->useit = true;
            if (tsfbp->skip)
                count_skip++;
            else
                count_noskip++;
        }
        if (count_noskip > 0) {
            for (tsfbp = sfbp; tsfbp; tsfbp = tsfbp->next) {
                if (NStr::CompareNocase(res->name, tsfbp->name) != 0)
                    continue;
                if (res != tsfbp && tsfbp->skip)
                    tsfbp->useit = false;
            }
        }
        return (SourceFeatDerive(sfbp, res));
    }

    for (tsfbp = sfbp; tsfbp; tsfbp = tsfbp->next) {
        if (! tsfbp->full)
            continue;
        res = tsfbp;
        break;
    }
    if (res) {
        sfbp = SourceFeatMoveOneUp(sfbp, res);
        return (SourceFeatRemoveDups(sfbp));
    }

    SourceFeatBlkSetFree(sfbp);
    FtaErrPost(SEV_ERROR, ERR_SOURCE_MissingSourceFeatureForDescr, "Could not select the right source feature among different organisms to create descriptor: no /focus and 1..N one. Entry dropped.");
    return nullptr;
}

/**********************************************************/
static void AddOrgMod(COrg_ref& org_ref, const Char* val, COrgMod::ESubtype type)
{
    COrgName& orgname = org_ref.SetOrgname();

    CRef<COrgMod> mod(new COrgMod);
    mod->SetSubtype(type);
    mod->SetSubname(val ? val : "");

    orgname.SetMod().push_back(mod);
}

/**********************************************************/
static void FTASubSourceAdd(CBioSource& bio, const Char* val, CSubSource::ESubtype type)
{
    if (type != CSubSource::eSubtype_dev_stage) {
        bool found = false;
        for (const auto& subtype : bio.GetSubtype()) {
            if (subtype->GetSubtype() == type) {
                found = true;
                break;
            }
        }

        if (found)
            return;
    }

    CRef<CSubSource> sub(new CSubSource);
    sub->SetSubtype(type);
    sub->SetName(val ? val : "");
    bio.SetSubtype().push_back(sub);
}

/**********************************************************/
static void CheckQualsInSourceFeat(CBioSource& bio, TQualVector& quals, Uint1 taxserver)
{
    const Char** b;

    char* p;

    if (! bio.CanGetOrg())
        return;

    vector<string> modnames;

    if (bio.GetOrg().CanGetOrgname() && bio.GetOrg().GetOrgname().CanGetMod()) {
        for (const auto& mod : bio.GetOrg().GetOrgname().GetMod()) {
            for (const auto& it : SourceOrgMods) {
                if (it.num != mod->GetSubtype())
                    continue;

                modnames.push_back(it.name);
                break;
            }
        }
    }

    for (const auto& cur : quals) {
        if (! cur->IsSetQual() || cur->GetQual() == "organism")
            continue;

        const string& cur_qual = cur->GetQual();
        const Char*   val_ptr  = cur->IsSetVal() ? cur->GetVal().c_str() : nullptr;

        if (cur_qual == "note") {
            FTASubSourceAdd(bio, val_ptr, CSubSource::eSubtype_other);
            continue;
        }

        for (b = SourceBadQuals; *b; b++) {
            if (cur_qual != *b)
                continue;

            if (! val_ptr || val_ptr[0] == '\0')
                p = StringSave("???");
            else
                p = StringSave(val_ptr);
            if (StringLen(p) > 50)
                p[50] = '\0';
            FtaErrPost(SEV_WARNING, ERR_SOURCE_UnwantedQualifiers, "Unwanted qualifier on source feature: {}={}", cur_qual, p);
            MemFree(p);
        }

        b = SourceSubSources;
        for (int i = CSubSource::eSubtype_chromosome; *b; i++, b++) {
            if (**b != '\0' && cur_qual == *b) {
                FTASubSourceAdd(bio, val_ptr, static_cast<CSubSource::ESubtype>(i));
                break;
            }
        }

        if (cur_qual == "organism" ||
            (taxserver != 0 && cur_qual == "type_material"))
            continue;

        if (find(modnames.begin(), modnames.end(), cur_qual) != modnames.end())
            continue;

        for (const auto& it : SourceOrgMods) {
            if (cur_qual == it.name) {
                AddOrgMod(bio.SetOrg(), val_ptr, it.num);
                break;
            }
        }
    }
}

/**********************************************************/
static CRef<CDbtag> GetSourceDbtag(CRef<CGb_qual>& qual, Parser::ESource source)
{
    CRef<CDbtag> tag;

    if (qual->GetQual() != "db_xref")
        return tag;

    const string& val_buf(qual->GetVal());

    auto pos = val_buf.find(':');
    if (pos == string::npos)
        return tag;

    string db(val_buf.substr(0, pos));
    string t(val_buf.substr(pos + 1));
    if (t.empty())
        return tag;

    if (NStr::CompareNocase(db, "taxon") == 0)
        return tag;

    const char* src;
    if (source == Parser::ESource::NCBI)
        src = "NCBI";
    else if (source == Parser::ESource::EMBL)
        src = "EMBL";
    else if (source == Parser::ESource::DDBJ)
        src = "DDBJ";
    else if (source == Parser::ESource::SPROT)
        src = "SwissProt";
    else if (source == Parser::ESource::LANL)
        src = "LANL";
    else if (source == Parser::ESource::Refseq)
        src = "RefSeq";
    else
        src = "Unknown";

    if (source != Parser::ESource::NCBI && source != Parser::ESource::DDBJ &&
        source != Parser::ESource::EMBL && source != Parser::ESource::LANL &&
        source != Parser::ESource::Refseq) {
        FtaErrPost(SEV_ERROR, ERR_SOURCE_InvalidDbXref, "Cannot process source feature's \"/db_xref={}\" for source \"{}\".", val_buf, src);
        return tag;
    }

    const char** b;
    for (b = ObsoleteSourceDbxrefTag; *b; b++) {
        if (NStr::CompareNocase(*b, db) == 0) {
            FtaErrPost(SEV_WARNING, ERR_SOURCE_ObsoleteDbXref, "/db_xref type \"{}\" is obsolete.", db);
            if (NStr::CompareNocase(db, "IFO") == 0) {
                db = "NBRC";
                qual->SetVal(db + ':' + t);
            }
            break;
        }
    }

    for (b = DENLRSourceDbxrefTag; *b; b++) {
        if (NStr::CompareNocase(*b, db) == 0)
            break;
    }

    if (! *b && (source == Parser::ESource::DDBJ || source == Parser::ESource::EMBL)) {
        for (b = DESourceDbxrefTag; *b; b++)
            if (NStr::CompareNocase(*b, db) == 0)
                break;
    }
    if (! *b && source == Parser::ESource::EMBL) {
        for (b = ESourceDbxrefTag; *b; b++)
            if (NStr::CompareNocase(*b, db) == 0)
                break;
    }
    if (! *b && (source == Parser::ESource::NCBI || source == Parser::ESource::LANL ||
                 source == Parser::ESource::Refseq)) {
        for (b = NLRSourceDbxrefTag; *b; b++) {
            if (NStr::CompareNocase(*b, db) == 0)
                break;
        }
    }

    if (! *b) {
        FtaErrPost(SEV_ERROR, ERR_SOURCE_InvalidDbXref, "Invalid database name in source feature's \"/db_xref={}\" for source \"{}\".", val_buf, src);
        return tag;
    }

    tag.Reset(new CDbtag);
    tag->SetDb(db);

    if (t.front() != '0' && t.find_first_not_of("0123456789") == string::npos)
        tag->SetTag().SetId(NStr::StringToInt(t, NStr::fConvErr_NoThrow));
    else
        tag->SetTag().SetStr(t);

    return tag;
}

/**********************************************************/
static bool UpdateRawBioSource(SourceFeatBlkPtr sfbp, Parser::ESource source, IndexblkPtr ibp, Uint1 taxserver)
{
    char* div;
    char* tco;
    char* p;
    char* q;

    CBioSource::TGenome newgen;
    CBioSource::TGenome oldgen;
    Int2 i;

    bool is_syn = false;
    bool is_pat = false;

    div = ibp->division;
    if (div) {
        if (StringEqu(div, "SYN"))
            is_syn = true;
        else if (StringEqu(div, "PAT"))
            is_pat = true;
    }
    for (; sfbp; sfbp = sfbp->next) {
        if (sfbp->bio_src.Empty())
            continue;

        CBioSource& bio = *sfbp->bio_src;

        if (! sfbp->lookup) {
            if (is_syn && ! sfbp->tg)
                bio.SetOrigin(CBioSource::eOrigin_artificial);
        } else {
            if (bio.CanGetOrg() && bio.GetOrg().CanGetOrgname() &&
                bio.GetOrg().GetOrgname().CanGetDiv() &&
                bio.GetOrg().GetOrgname().GetDiv() == "SYN") {
                bio.SetOrigin(CBioSource::eOrigin_artificial);
                if (is_syn == false && is_pat == false) {
                    const Char* taxname = nullptr;
                    if (bio.GetOrg().CanGetTaxname() &&
                        ! bio.GetOrg().GetTaxname().empty())
                        taxname = bio.GetOrg().GetTaxname().c_str();
                    FtaErrPost(SEV_ERROR, ERR_ORGANISM_SynOrgNameNotSYNdivision, "The NCBI Taxonomy DB believes that organism name \"{}\" is reserved for synthetic sequences, but this record is not in the SYN division.", taxname ? taxname : "not_specified");
                }
            }
        }

        newgen = -1;
        oldgen = -1;

        bool dropped = false;
        for (auto& cur : sfbp->quals) {
            if (! cur->IsSetQual() || cur->GetQual().empty())
                continue;

            const string& cur_qual = cur->GetQual();
            string cq = cur_qual;
            if (cq == "geo_loc_name") {
                cq = "country";
            }
            if (cq == "db_xref") {
                CRef<CDbtag> dbtag = GetSourceDbtag(cur, source);
                if (dbtag.Empty())
                    continue;

                bio.SetOrg().SetDb().push_back(dbtag);
                continue;
            }

            const Char* val_ptr = cur->IsSetVal() ? cur->GetVal().c_str() : nullptr;
            if (cq == "organelle") {
                if (! val_ptr || val_ptr[0] == '\0')
                    continue;

                const char* p = StringChr(val_ptr, ':');
                if (p) {
                    if (StringChr(p + 1, ':')) {
                        FtaErrPost(SEV_ERROR, ERR_SOURCE_OrganelleQualMultToks, "More than 2 tokens found in /organelle qualifier: \"{}\". Entry dropped.", val_ptr);
                        dropped = true;
                        break;
                    }

                    string_view val_str(val_ptr, p - val_ptr);
                    i = StringMatchIcase(OrganelleFirstToken, val_str);
                    if (i < 0) {
                        FtaErrPost(SEV_ERROR, ERR_SOURCE_OrganelleIllegalClass, "Illegal class in /organelle qualifier: \"{}\". Entry dropped.", val_ptr);
                        dropped = true;
                        break;
                    }
                    if (i == 4)
                        ibp->got_plastid = true;
                    if (newgen < 0)
                        newgen = StringMatchIcase(GenomicSourceFeatQual,
                                                  p + 1);
                } else {
                    i = StringMatchIcase(OrganelleFirstToken, val_ptr);
                    if (i < 0) {
                        FtaErrPost(SEV_ERROR, ERR_SOURCE_OrganelleIllegalClass, "Illegal class in /organelle qualifier: \"{}\". Entry dropped.", val_ptr);
                        dropped = true;
                        break;
                    }
                    if (i == 4)
                        ibp->got_plastid = true;
                    if (newgen < 0)
                        newgen = StringMatchIcase(GenomicSourceFeatQual, val_ptr);
                }
                continue;
            }

            if (oldgen < 0)
                oldgen = StringMatchIcase(GenomicSourceFeatQual, cq);

            if (cq != "country" ||
                ! val_ptr || val_ptr[0] == '\0')
                continue;

            tco = StringSave(val_ptr);
            p   = StringChr(tco, ':');
            if (p)
                *p = '\0';
            for (p = tco; *p == ' ' || *p == '\t';)
                p++;
            if (*p == '\0') {
                FtaErrPost(SEV_ERROR, ERR_SOURCE_InvalidCountry, "Empty country name in /{} qualifier : \"{}\".", cur_qual, val_ptr);
            } else {
                for (q = p + 1; *q != '\0';)
                    q++;
                for (q--; *q == ' ' || *q == '\t';)
                    q--;
                *++q = '\0';

                if(MatchArrayString(NullTermValues, val_ptr) < 0)
                {
                    bool valid_country = CCountries::IsValid(p);
                    if (! valid_country) {
                        valid_country = CCountries::WasValid(p);

                        if (! valid_country)
                            FtaErrPost(SEV_ERROR, ERR_SOURCE_InvalidCountry, "Country \"{}\" from /{} qualifier \"{}\" is not a valid country name.", tco, cur_qual, val_ptr);
                        else
                            FtaErrPost(SEV_WARNING, ERR_SOURCE_FormerCountry, "Country \"{}\" from /{} qualifier \"{}\" is a former country name which is no longer valid.", tco, cur_qual, val_ptr);
                    }
                }
            }

            MemFree(tco);
            FTASubSourceAdd(bio, val_ptr, CSubSource::eSubtype_country);
        }

        if (dropped)
            break;

        if (newgen > -1)
            bio.SetGenome(newgen);
        else if (oldgen > -1)
            bio.SetGenome(oldgen);
        else if (sfbp->genome != CBioSource::eGenome_unknown)
            bio.SetGenome(sfbp->genome);

        CheckQualsInSourceFeat(bio, sfbp->quals, taxserver);
        fta_sort_biosource(bio);
    }

    if (sfbp)
        return false;

    return true;
}


/**********************************************************/
static bool is_a_space_char(Char c)
{
    return c == ' ' || c == '\t';
}

/**********************************************************/
static void CompareDescrFeatSources(SourceFeatBlkPtr sfbp, const CBioseq& bioseq)
{
    SourceFeatBlkPtr tsfbp;

    if (! sfbp || ! bioseq.IsSetDescr())
        return;

    for (const auto& descr : bioseq.GetDescr().Get()) {
        if (! descr->IsSource())
            continue;

        const CBioSource& bio_src = descr->GetSource();

        if (! bio_src.IsSetOrg() || ! bio_src.GetOrg().IsSetTaxname() ||
            bio_src.GetOrg().GetTaxname().empty())
            continue;

        const string& taxname = bio_src.GetOrg().GetTaxname();
        string        orgdescr;
        std::remove_copy_if(taxname.begin(), taxname.end(), std::back_inserter(orgdescr), is_a_space_char);

        string commdescr;
        if (bio_src.GetOrg().IsSetCommon()) {
            const string& common = bio_src.GetOrg().GetCommon();
            std::remove_copy_if(common.begin(), common.end(), std::back_inserter(commdescr), is_a_space_char);
        }

        for (tsfbp = sfbp; tsfbp; tsfbp = tsfbp->next) {
            if (tsfbp->name == nullptr || tsfbp->name[0] == '\0')
                continue;

            size_t name_len = strlen(tsfbp->name);
            string orgfeat;
            std::remove_copy_if(tsfbp->name, tsfbp->name + name_len, std::back_inserter(orgfeat), is_a_space_char);

            if (NStr::CompareNocase(orgdescr.c_str(), "unknown") == 0) {
                if (NStr::CompareNocase(orgdescr, orgfeat) == 0 ||
                    (! commdescr.empty() && NStr::CompareNocase(commdescr, orgfeat) == 0)) {
                    break;
                }
            } else {
                if (orgdescr == orgfeat || commdescr == orgfeat) {
                    break;
                }
            }
        }

        if (! tsfbp) {
            FtaErrPost(SEV_ERROR, ERR_ORGANISM_NoSourceFeatMatch, "Organism name \"{}\" from OS/ORGANISM line does not exist in this record's source features.", taxname);
        }
    }
}

/**********************************************************/
static bool CheckSourceLineage(SourceFeatBlkPtr sfbp, Parser::ESource source, bool is_pat)
{
    const Char* p;
    ErrSev      sev;

    for (; sfbp; sfbp = sfbp->next) {
        if (! sfbp->lookup || sfbp->bio_src.Empty() || ! sfbp->bio_src->IsSetOrg())
            continue;

        p = nullptr;
        if (sfbp->bio_src->GetOrg().IsSetOrgname() &&
            sfbp->bio_src->GetOrg().GetOrgname().IsSetLineage())
            p = sfbp->bio_src->GetOrg().GetOrgname().GetLineage().c_str();

        if (! p || *p == '\0') {
            if ((source == Parser::ESource::DDBJ || source == Parser::ESource::EMBL) && is_pat)
                sev = SEV_WARNING;
            else
                sev = SEV_REJECT;
            FtaErrPost(sev, ERR_SERVER_NoLineageFromTaxon, "Taxonomy lookup for organism name \"{}\" yielded an Org-ref that has no lineage.", sfbp->name);
            if (sev == SEV_REJECT)
                break;
        }
    }
    if (! sfbp)
        return true;
    return false;
}

/**********************************************************/
static void PropagateSuppliedLineage(const CBioseq& bioseq,
                                     SourceFeatBlkPtr sfbp,
                                     Uint1            taxserver)
{
    SourceFeatBlkPtr tsfbp;

    const Char* p;

    if (! bioseq.IsSetDescr() || ! sfbp)
        return;

    for (; sfbp; sfbp = sfbp->next) {
        if (sfbp->lookup || sfbp->bio_src.Empty() ||
            ! sfbp->bio_src->IsSetOrg() || ! sfbp->bio_src->GetOrg().IsSetTaxname() ||
            ! sfbp->name || *sfbp->name == '\0' ||
            sfbp->bio_src->GetOrg().GetTaxname().empty())
            continue;

        COrgName& orgname = sfbp->bio_src->SetOrg().SetOrgname();

        if (orgname.IsSetLineage()) {
            if (! orgname.GetLineage().empty())
                continue;

            orgname.ResetLineage();
        }

        const string& taxname = sfbp->bio_src->GetOrg().GetTaxname();
        string        lineage;

        bool found = false;
        for (const auto& descr : bioseq.GetDescr().Get()) {
            if (! descr->IsSource())
                continue;

            const CBioSource& bio_src = descr->GetSource();

            if (! bio_src.IsSetOrg() || ! bio_src.GetOrg().IsSetOrgname() ||
                ! bio_src.GetOrg().IsSetTaxname() || bio_src.GetOrg().GetTaxname().empty() ||
                ! bio_src.GetOrg().GetOrgname().IsSetLineage())
                continue;

            lineage                   = bio_src.GetOrg().GetOrgname().GetLineage();
            const string& cur_taxname = bio_src.GetOrg().GetTaxname();

            if (NStr::CompareNocase(cur_taxname, taxname) == 0) {
                found = true;
                break;
            }
        }

        if (! found) {
            FtaErrPost((taxserver == 0) ? SEV_INFO : SEV_WARNING,
                      ERR_ORGANISM_UnclassifiedLineage,
                      "Taxonomy lookup for organism name \"%s\" failed, and no matching organism exists in OS/ORGANISM lines, so lineage has been set to \"Unclassified\".",
                      taxname.c_str());
            p = "Unclassified";
        } else {
            if (lineage.empty()) {
                FtaErrPost((taxserver == 0) ? SEV_INFO : SEV_WARNING,
                          ERR_ORGANISM_UnclassifiedLineage,
                          "Taxonomy lookup for organism name \"%s\" failed, and the matching organism from OS/ORGANISM lines has no lineage, so lineage has been set to \"Unclassified\".",
                          taxname.c_str());
                p = "Unclassified";
            } else
                p = lineage.c_str();
        }

        orgname.SetLineage(p);
        for (tsfbp = sfbp->next; tsfbp; tsfbp = tsfbp->next) {
            if (tsfbp->lookup || tsfbp->bio_src.Empty() ||
                ! tsfbp->bio_src->IsSetOrg() || ! tsfbp->bio_src->GetOrg().IsSetTaxname() ||
                ! tsfbp->name || *tsfbp->name == '\0' ||
                tsfbp->bio_src->GetOrg().GetTaxname().empty() ||
                NStr::CompareNocase(sfbp->name, tsfbp->name) != 0)

                continue;

            COrgName& torgname = tsfbp->bio_src->SetOrg().SetOrgname();

            if (torgname.IsSetLineage()) {
                if (! torgname.GetLineage().empty())
                    continue;
            }
            torgname.SetLineage(p);
        }
    }
}

/**********************************************************/
static bool CheckMoltypeConsistency(SourceFeatBlkPtr sfbp, string& moltype)
{
    SourceFeatBlkPtr tsfbp;
    char*            name;
    char*            p;
    bool             ret;
    Char             ch;

    if (! sfbp)
        return true;

    for (tsfbp = sfbp; tsfbp; tsfbp = tsfbp->next)
        if (tsfbp->moltype)
            break;

    if (! tsfbp)
        return true;

    name = tsfbp->moltype;
    for (ret = true, tsfbp = sfbp; tsfbp; tsfbp = tsfbp->next) {
        if (! tsfbp->moltype) {
            ch = '\0';
            p  = tsfbp->location;
            if (p && StringLen(p) > 50) {
                ch    = p[50];
                p[50] = '\0';
            }
            FtaErrPost(SEV_ERROR, ERR_SOURCE_MissingMolType, "Source feature at \"{}\" lacks a /mol_type qualifier.", p ? p : "<empty>");
            if (ch != '\0')
                p[50] = ch;
        } else if (ret && ! StringEqu(name, tsfbp->moltype))
            ret = false;
    }

    if (ret)
        moltype = name;

    return (ret);
}

/**********************************************************/
static bool CheckForENV(SourceFeatBlkPtr sfbp, IndexblkPtr ibp, Parser::ESource source)
{
    const char** b;

    char* location;
    Int4  sources;
    Int4  envs;
    Char  ch;

    if (! sfbp || ! ibp)
        return true;

    bool skip            = false;
    location             = nullptr;
    ibp->env_sample_qual = false;
    for (envs = 0, sources = 0; sfbp; sfbp = sfbp->next, sources++) {
        bool env_found = false;
        for (const auto& cur : sfbp->quals) {
            if (cur->IsSetQual() && cur->GetQual() == "environmental_sample") {
                env_found = true;
                break;
            }
        }
        if (env_found)
            envs++;
        else
            location = sfbp->location;

        if (! sfbp->full || ! sfbp->name || sfbp->name[0] == '\0')
            continue;

        for (b = special_orgs; *b; b++) {
            if (NStr::CompareNocase(*b, sfbp->name) == 0)
                break;
        }
        if (*b)
            skip = true;
    }

    if (envs > 0) {
        ibp->env_sample_qual = true;
        if (! skip && envs != sources) {
            if (location && StringLen(location) > 50) {
                ch           = location[50];
                location[50] = '\0';
            } else
                ch = '\0';
            FtaErrPost(SEV_REJECT, ERR_SOURCE_InconsistentEnvSampQual, "Inconsistent /environmental_sample qualifier usage. Source feature at location \"{}\" lacks the qualifier.", location ? location : "unknown");
            if (ch != '\0')
                location[50] = ch;
            return false;
        }
    } else if (NStr::CompareNocase(ibp->division, "ENV") == 0) {
        if (source == Parser::ESource::EMBL)
            FtaErrPost(SEV_ERROR, ERR_SOURCE_MissingEnvSampQual, "This ENV division record has source features that lack the /environmental_sample qualifier. It will not be placed in the ENV division until the qualifier is added.");
        else {
            FtaErrPost(SEV_REJECT, ERR_SOURCE_MissingEnvSampQual, "This ENV division record has source features that lack the /environmental_sample qualifier.");
            return false;
        }
    }
    return true;
}

/**********************************************************/
static char* CheckPcrPrimersTag(char* str)
{
    if (StringEquN(str, "fwd_name", 8) ||
        StringEquN(str, "rev_name", 8))
        str += 8;
    else if (StringEquN(str, "fwd_seq", 7) ||
             StringEquN(str, "rev_seq", 7))
        str += 7;
    else
        return nullptr;

    if (*str == ' ')
        str++;
    if (*str == ':')
        return (str + 1);
    return nullptr;
}

/**********************************************************/
static void PopulatePcrPrimers(CBioSource& bio, PcrPrimersPtr ppp, Int4 count)
{
    PcrPrimersPtr tppp = nullptr;

    string str_fs;
    string str_rs;
    string str_fn;
    string str_rn;
    Int4   num_fn;
    Int4   num_rn;

    if (! ppp || count < 1)
        return;

    CBioSource::TSubtype& subs = bio.SetSubtype();
    CRef<CSubSource>      sub;

    if (count == 1) {
        sub.Reset(new CSubSource);
        sub->SetSubtype(CSubSource::eSubtype_fwd_primer_seq);
        sub->SetName(ppp->fwd_seq);
        subs.push_back(sub);

        sub.Reset(new CSubSource);
        sub->SetSubtype(CSubSource::eSubtype_rev_primer_seq);
        sub->SetName(ppp->rev_seq);
        subs.push_back(sub);

        if (ppp->fwd_name && ppp->fwd_name[0] != '\0') {
            sub.Reset(new CSubSource);
            sub->SetSubtype(CSubSource::eSubtype_fwd_primer_name);
            sub->SetName(ppp->fwd_name);
            subs.push_back(sub);
        }

        if (ppp->rev_name && ppp->rev_name[0] != '\0') {
            sub.Reset(new CSubSource);
            sub->SetSubtype(CSubSource::eSubtype_rev_primer_name);
            sub->SetName(ppp->rev_name);
            subs.push_back(sub);
        }
        return;
    }

    size_t len_fs = 1,
           len_rs = 1,
           len_fn = 0,
           len_rn = 0;
    num_fn        = 0;
    num_rn        = 0;
    for (tppp = ppp; tppp; tppp = tppp->next) {
        len_fs += (StringLen(tppp->fwd_seq) + 1);
        len_rs += (StringLen(tppp->rev_seq) + 1);
        if (tppp->fwd_name && tppp->fwd_name[0] != '\0') {
            len_fn += (StringLen(tppp->fwd_name) + 1);
            num_fn++;
        }
        if (tppp->rev_name && tppp->rev_name[0] != '\0') {
            len_rn += (StringLen(tppp->rev_name) + 1);
            num_rn++;
        }
    }

    str_fs.reserve(len_fs);
    str_rs.reserve(len_rs);
    if (len_fn > 0)
        str_fn.reserve(len_fn + count - num_fn + 1);
    if (len_rn > 0)
        str_rn.reserve(len_rn + count - num_rn + 1);

    for (tppp = ppp; tppp; tppp = tppp->next) {
        str_fs.append(",");
        str_fs.append(tppp->fwd_seq);
        str_rs.append(",");
        str_rs.append(tppp->rev_seq);
        if (len_fn > 0) {
            str_fn.append(",");
            if (tppp->fwd_name && tppp->fwd_name[0] != '\0')
                str_fn.append(tppp->fwd_name);
        }
        if (len_rn > 0) {
            str_rn.append(",");
            if (tppp->rev_name && tppp->rev_name[0] != '\0')
                str_rn.append(tppp->rev_name);
        }
    }

    if (! str_fs.empty()) {
        str_fs[0] = '(';
        str_fs += ')';
    }

    sub.Reset(new CSubSource);
    sub->SetSubtype(CSubSource::eSubtype_fwd_primer_seq);
    sub->SetName(str_fs);
    subs.push_back(sub);

    if (! str_rs.empty()) {
        str_rs[0] = '(';
        str_rs += ')';
    }

    sub.Reset(new CSubSource);
    sub->SetSubtype(CSubSource::eSubtype_rev_primer_seq);
    sub->SetName(str_rs);
    subs.push_back(sub);

    if (! str_fn.empty()) {
        str_fn[0] = '(';
        str_fn += ')';

        sub.Reset(new CSubSource);
        sub->SetSubtype(CSubSource::eSubtype_fwd_primer_name);
        sub->SetName(str_fn);
        subs.push_back(sub);
    }

    if (! str_rn.empty()) {
        str_rn[0] = '(';
        str_rn += ')';

        sub.Reset(new CSubSource);
        sub->SetSubtype(CSubSource::eSubtype_rev_primer_name);
        sub->SetName(str_rn);
        subs.push_back(sub);
    }
}

/**********************************************************/
static void PcrPrimersFree(PcrPrimersPtr ppp)
{
    PcrPrimersPtr next;

    for (; ppp; ppp = next) {
        next = ppp->next;
        if (ppp->fwd_name)
            MemFree(ppp->fwd_name);
        if (ppp->fwd_seq)
            MemFree(ppp->fwd_seq);
        if (ppp->rev_name)
            MemFree(ppp->rev_name);
        if (ppp->rev_seq)
            MemFree(ppp->rev_seq);
        delete ppp;
    }
}

/**********************************************************/
static bool ParsePcrPrimers(SourceFeatBlkPtr sfbp)
{
    PcrPrimersPtr ppp;
    PcrPrimersPtr tppp = nullptr;

    char* p;
    char* q;
    char* r;
    bool  comma;
    bool  bad_start;
    bool  empty;
    Char  ch = '\0';
    Int4  count;
    Int4  prev; /* 1 = fwd_name, 2 = fwd_seq,
                                           3 = rev_name, 4 = rev_seq */

    bool got_problem = false;
    for (ppp = nullptr; sfbp; sfbp = sfbp->next) {
        if (sfbp->quals.empty() || sfbp->bio_src.Empty())
            continue;

        count = 0;
        for (const auto& cur : sfbp->quals) {
            if (cur->GetQual() != "PCR_primers" ||
                ! cur->IsSetVal() || cur->GetVal().empty())
                continue;

            count++;
            if (! ppp) {
                ppp  = new PcrPrimers;
                tppp = ppp;
            } else {
                tppp->next = new PcrPrimers;
                tppp       = tppp->next;
            }

            prev = 0;
            std::vector<Char> val_buf(cur->GetVal().begin(), cur->GetVal().end());
            val_buf.push_back(0);

            for (comma = false, bad_start = false, p = &val_buf[0]; *p != '\0';) {
                q = CheckPcrPrimersTag(p);
                if (! q) {
                    if (p != &val_buf[0]) {
                        p++;
                        continue;
                    }
                    bad_start = true;
                    break;
                }

                if (*q == ' ')
                    q++;
                for (r = q;;) {
                    r = StringChr(r, ',');
                    if (! r)
                        break;
                    if (*++r == ' ')
                        r++;
                    if (CheckPcrPrimersTag(r))
                        break;
                }
                if (r) {
                    r--;
                    if (*r == ' ')
                        r--;
                    if (r > q && *(r - 1) == ' ')
                        r--;
                    ch = *r;
                    *r = '\0';
                }

                if (StringChr(q, ','))
                    comma = true;

                empty = false;
                if (! q || *q == '\0')
                    empty = true;
                else if (StringEquN(p, "fwd_name", 8)) {
                    if (prev == 1)
                        prev = -2;
                    else if (prev > 2 && prev < 5)
                        prev = -1;
                    else {
                        if (! tppp->fwd_name)
                            tppp->fwd_name = StringSave(q);
                        else {
                            string s(tppp->fwd_name);
                            s.append(":");
                            s.append(q);
                            MemFree(tppp->fwd_name);
                            tppp->fwd_name = StringSave(s);
                        }
                        prev = 1;
                    }
                } else if (StringEquN(p, "fwd_seq", 7)) {
                    if (prev > 2 && prev < 5)
                        prev = -1;
                    else {
                        if (! tppp->fwd_seq)
                            tppp->fwd_seq = StringSave(q);
                        else {
                            string s(tppp->fwd_seq);
                            s.append(":");
                            s.append(q);
                            MemFree(tppp->fwd_seq);
                            tppp->fwd_seq = StringSave(s);
                            if (prev != 1) {
                                if (! tppp->fwd_name)
                                    tppp->fwd_name = StringSave(":");
                                else {
                                    string s(tppp->fwd_name);
                                    s.append(":");
                                    MemFree(tppp->fwd_name);
                                    tppp->fwd_name = StringSave(s);
                                }
                            }
                        }
                        prev = 2;
                    }
                } else if (StringEquN(p, "rev_name", 8)) {
                    if (prev == 3 || prev == 1)
                        prev = -2;
                    else {
                        if (! tppp->rev_name)
                            tppp->rev_name = StringSave(q);
                        else {
                            string s(tppp->rev_name);
                            s.append(":");
                            s.append(q);
                            MemFree(tppp->rev_name);
                            tppp->rev_name = StringSave(s);
                        }
                        prev = 3;
                    }
                } else {
                    if (prev == 1)
                        prev = -2;
                    else {
                        if (! tppp->rev_seq)
                            tppp->rev_seq = StringSave(q);
                        else {
                            string s(tppp->rev_seq);
                            s.append(":");
                            s.append(q);
                            MemFree(tppp->rev_seq);
                            tppp->rev_seq = StringSave(s);
                            if (prev != 3) {
                                if (! tppp->rev_name)
                                    tppp->rev_name = StringSave(":");
                                else {
                                    string s(tppp->rev_name);
                                    s.append(":");
                                    MemFree(tppp->rev_name);
                                    tppp->rev_name = StringSave(s);
                                }
                            }
                        }
                        prev = 4;
                    }
                }

                if (! r)
                    break;

                *r++ = ch;

                if (comma || prev < 0 || empty)
                    break;

                if (ch == ' ')
                    r++;
                if (*r == ' ')
                    r++;
                p = r;
            }

            if (prev == 1 || prev == 3)
                prev = -2;

            if (bad_start) {
                FtaErrPost(SEV_REJECT, ERR_QUALIFIER_InvalidPCRprimer, "Unknown text found at the beginning of /PCR_primers qualifier: \"{}\". Entry dropped.", &val_buf[0]);
                got_problem = true;
                break;
            }

            if (comma) {
                FtaErrPost(SEV_REJECT, ERR_QUALIFIER_PCRprimerEmbeddedComma, "Encountered embedded comma within /PCR_primers qualifier's field value: \"{}\". Entry dropped.", &val_buf[0]);
                got_problem = true;
                break;
            }

            if (prev == -1) {
                FtaErrPost(SEV_REJECT, ERR_QUALIFIER_InvalidPCRprimer, "Encountered incorrect order of \"forward\" and \"reversed\" sequences within /PCR_primers qualifier: \"{}\". Entry dropped.", &val_buf[0]);
                got_problem = true;
                break;
            }

            if (prev == -2) {
                FtaErrPost(SEV_REJECT, ERR_QUALIFIER_MissingPCRprimerSeq, "/PCR_primers qualifier \"{}\" is missing or has an empty required fwd_seq or rev_seq fields (or both). Entry dropped.", &val_buf[0]);
                got_problem = true;
                break;
            }

            if (empty) {
                FtaErrPost(SEV_REJECT, ERR_QUALIFIER_InvalidPCRprimer, "/PCR_primers qualifier \"{}\" has an empty field value. Entry dropped.", &val_buf[0]);
                got_problem = true;
                break;
            }

            if (! tppp->fwd_seq || tppp->fwd_seq[0] == '\0' ||
                ! tppp->rev_seq || tppp->rev_seq[0] == '\0') {
                FtaErrPost(SEV_REJECT, ERR_QUALIFIER_MissingPCRprimerSeq, "/PCR_primers qualifier \"{}\" is missing or has an empty required fwd_seq or rev_seq fields (or both). Entry dropped.", &val_buf[0]);
                got_problem = true;
                break;
            }
        }

        if (got_problem) {
            PcrPrimersFree(ppp);
            break;
        }

        PopulatePcrPrimers(*sfbp->bio_src, ppp, count);
        PcrPrimersFree(ppp);
        ppp = nullptr;
    }

    if (! sfbp)
        return true;
    return false;
}

/**********************************************************/
static void CheckCollectionDate(SourceFeatBlkPtr sfbp, Parser::ESource source)
{
    const char*  Mmm[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", nullptr };
    const char** b;
    const char*  q;

    char*  p;
    char*  r;
    char*  val;
    Int4   year;
    Int4   month;
    Int4   day;
    Int4   bad;
    Int4   num_slash;
    Int4   num_T;
    Int4   num_colon;
    Int4   num_Z;
    size_t len;

    CTime     time(CTime::eCurrent);
    CDate_std date(time);

    for (; sfbp; sfbp = sfbp->next) {
        if (sfbp->quals.empty() || sfbp->bio_src.Empty())
            continue;

        for (const auto& cur : sfbp->quals) {
            bad = 0;
            if (cur->GetQual() != "collection_date" ||
                ! cur->IsSetVal() || cur->GetVal().empty())
                continue;

            val = (char*)cur->GetVal().c_str();

            if(MatchArrayString(NullTermValues, val) > -1)
                continue;

            for (num_slash = 0, p = val; *p != '\0'; p++)
                if (*p == '/')
                    num_slash++;

            if (num_slash > 1) {
                p = StringSave(sfbp->location);
                if (p && StringLen(p) > 50)
                    p[50] = '\0';
                FtaErrPost(SEV_ERROR, ERR_SOURCE_InvalidCollectionDate, "/collection_date \"{}\" for source feature at \"{}\" has too many components.", val, p ? p : "unknown location");
                if (p)
                    MemFree(p);
                continue;
            }

            for (val = (char*)cur->GetVal().c_str();;) {
                r = StringChr(val, '/');
                if (r)
                    *r = '\0';

                len = StringLen(val);

                if (len == 4) {
                    for (q = val; *q == '0';)
                        q++;
                    for (p = (char*)q; *p != '\0'; p++)
                        if (*p < '0' || *p > '9')
                            break;
                    if (*p != '\0')
                        bad = 1;
                    else if (atoi(q) > date.GetYear())
                        bad = 3;
                } else if (len == 8) {
                    if (val[3] != '-')
                        bad = 1;
                    else {
                        p    = val;
                        p[3] = '\0';
                        if (source == Parser::ESource::DDBJ) {
                            if (p[0] >= 'a' && p[0] <= 'z')
                                p[0] &= ~040;
                            if (p[1] >= 'A' && p[1] <= 'Z')
                                p[1] |= 040;
                            if (p[2] >= 'A' && p[2] <= 'Z')
                                p[2] |= 040;
                        }
                        for (b = Mmm, month = 1; *b; b++, month++)
                            if (StringEqu(*b, p))
                                break;
                        if (! *b)
                            bad = 1;
                        p[3] = '-';
                    }
                    if (bad == 0) {
                        for (q = val + 4; *q == '0';)
                            q++;
                        for (p = (char*)q; *p != '\0'; p++)
                            if (*p < '0' || *p > '9')
                                break;
                        if (*p != '\0')
                            bad = 1;
                        else {
                            year = atoi(q);
                            if (year > date.GetYear() ||
                                (year == date.GetYear() && month > date.GetMonth()))
                                bad = 3;
                        }
                    }
                } else if (len == 11) {
                    if (val[2] != '-' || val[6] != '-')
                        bad = 1;
                    else {
                        p      = val;
                        val[2] = '\0';
                        val[6] = '\0';
                        if (p[0] < '0' || p[0] > '3' || p[1] < '0' || p[1] > '9')
                            bad = 1;
                        else {
                            if (*p == '0')
                                p++;
                            day = atoi(p);
                            p   = val + 3;
                            if (source == Parser::ESource::DDBJ) {
                                if (p[0] >= 'a' && p[0] <= 'z')
                                    p[0] &= ~040;
                                if (p[1] >= 'A' && p[1] <= 'Z')
                                    p[1] |= 040;
                                if (p[2] >= 'A' && p[2] <= 'Z')
                                    p[2] |= 040;
                            }
                            for (b = Mmm, month = 1; *b; b++, month++)
                                if (StringEqu(*b, p))
                                    break;
                            if (! *b)
                                bad = 1;
                            else {
                                if (day < 1 || day > 31)
                                    bad = 2;
                                else if (month == 2 && day > 29)
                                    bad = 2;
                                else if ((month == 4 || month == 6 || month == 9 || month == 11) && day > 30)
                                    bad = 2;
                            }
                        }
                        if (bad == 0) {
                            for (q = val + 7; *q == '0';)
                                q++;
                            for (p = (char*)q; *p != '\0'; p++)
                                if (*p < '0' || *p > '9')
                                    break;
                            if (*p != '\0')
                                bad = 1;
                            else {
                                year = atoi(q) - 1900;
                                if (year > date.GetYear() ||
                                    (year == date.GetYear() && month > date.GetMonth()) ||
                                    (year == date.GetYear() && month == date.GetMonth() && day > date.GetDay()))
                                    bad = 3;
                            }
                        }
                        val[2] = '-';
                        val[6] = '-';
                    }
                } else if (len == 7 || len == 10 || len == 14 || len == 17 ||
                           len == 20) {
                    num_T     = 0;
                    num_Z     = 0;
                    num_colon = 0;
                    for (p = val; *p != '\0'; p++) {
                        if ((*p < 'a' || *p > 'z') && (*p < 'A' || *p > 'Z') &&
                            (*p < '0' || *p > '9') && *p != '-' && *p != '/' &&
                            *p != ':') {
                            bad = 3;
                            break;
                        }
                        if (*p == ':')
                            num_colon++;
                        else if (*p == 'T')
                            num_T++;
                        else if (*p == 'Z')
                            num_Z++;
                    }
                    if (len == 7 || len == 10) {
                        if (num_T > 0)
                            bad = 4;
                        if (num_Z > 0)
                            bad = 5;
                        if (num_colon > 0)
                            bad = 6;
                    } else {
                        if (num_Z > 1)
                            bad = 5;
                        if (num_T > 1)
                            bad = 4;
                        if ((len == 14 && num_colon > 0) ||
                            (len == 17 && num_colon > 1) ||
                            (len == 20 && num_colon > 2))
                            bad = 6;
                    }
                } else
                    bad = 8;

                if (bad == 0) {
                    if (! r)
                        break;

                    *r  = '/';
                    val = r + 1;
                    continue;
                }

                p = StringSave(sfbp->location);
                if (p && StringLen(p) > 50)
                    p[50] = '\0';
                if (bad == 1)
                    q = "is not of the format DD-Mmm-YYYY, Mmm-YYYY, or YYYY";
                else if (bad == 2)
                    q = "has an illegal day value for the stated month";
                else if (bad == 3)
                    q = "has invalid characters";
                else if (bad == 4)
                    q = "has too many time values";
                else if (bad == 5)
                    q = "has too many Zulu indicators";
                else if (bad == 6)
                    q = "has too many hour and minute delimiters";
                else
                    q = "has not yet occured";
                FtaErrPost(SEV_ERROR, ERR_SOURCE_InvalidCollectionDate, "/collection_date \"{}\" for source feature at \"{}\" {}.", val, p ? p : "unknown location", q);
                if (p)
                    MemFree(p);

                if (! r)
                    break;

                *r  = '/';
                val = r + 1;
            }
        }
    }
}

/**********************************************************/
static bool CheckNeedSYNFocus(SourceFeatBlkPtr sfbp)
{
    const char** b;

    if (! sfbp || ! sfbp->next)
        return false;

    for (; sfbp; sfbp = sfbp->next) {
        if (! sfbp->full)
            continue;

        for (b = special_orgs; *b; b++)
            if (NStr::CompareNocase(*b, sfbp->name) == 0)
                break;

        if (*b)
            break;
    }

    if (sfbp)
        return false;
    return true;
}

/**********************************************************/
static void CheckMetagenome(CBioSource& bio)
{
    if (! bio.IsSetOrg())
        return;

    bool metatax = false;
    bool metalin = false;

    if (bio.IsSetOrgname() && bio.GetOrgname().IsSetLineage() &&
        bio.GetOrgname().GetLineage().find("metagenomes") != string::npos)
        metalin = true;

    if (bio.GetOrg().IsSetTaxname() &&
        bio.GetOrg().GetTaxname().find("metagenome") != string::npos)
        metatax = true;

    if (! metalin && ! metatax)
        return;

    const Char* taxname = bio.GetOrg().IsSetTaxname() ? bio.GetOrg().GetTaxname().c_str() : nullptr;
    if (! taxname || taxname[0] == 0)
        taxname = "unknown";

    if (metalin && metatax) {
        CRef<CSubSource> sub(new CSubSource);
        sub->SetSubtype(CSubSource::eSubtype_metagenomic);
        sub->SetName("");
        bio.SetSubtype().push_back(sub);
    } else if (! metalin)
        FtaErrPost(SEV_ERROR, ERR_ORGANISM_LineageLacksMetagenome, "Organism name \"{}\" contains \"metagenome\" but the lineage lacks the \"metagenomes\" classification.", taxname);
    else
        FtaErrPost(SEV_ERROR, ERR_ORGANISM_OrgNameLacksMetagenome, "Lineage includes the \"metagenomes\" classification but organism name \"{}\" lacks \"metagenome\".", taxname);
}

/**********************************************************/
static bool CheckSubmitterSeqidQuals(SourceFeatBlkPtr sfbp, char* acc)
{
    SourceFeatBlkPtr tsfbp;
    char*            ssid;
    Int4             count_feat;
    Int4             count_qual;

    if (! sfbp)
        return (true);

    count_feat = 0;
    count_qual = 0;
    for (ssid = nullptr, tsfbp = sfbp; tsfbp; tsfbp = tsfbp->next) {
        count_feat++;
        if (! tsfbp->submitter_seqid)
            continue;

        count_qual++;
        if (tsfbp->submitter_seqid[0] == '\0') {
            FtaErrPost(SEV_REJECT, ERR_SOURCE_MultipleSubmitterSeqids, "Multiple /submitter_seqid qualifiers were encountered within source feature at location \"{}\". Entry dropped.", tsfbp->location ? tsfbp->location : "?empty?");
            break;
        }

        if (! ssid)
            ssid = tsfbp->submitter_seqid;
        else if (! StringEqu(ssid, tsfbp->submitter_seqid)) {
            FtaErrPost(SEV_REJECT, ERR_SOURCE_DifferentSubmitterSeqids, "Different /submitter_seqid qualifiers were encountered amongst source features: \"{}\" and \"{}\" at least. Entry dropped.", ssid, tsfbp->submitter_seqid);
            break;
        }
    }

    if (tsfbp)
        return (false);

    if (count_feat == count_qual)
        return (true);

    FtaErrPost(SEV_REJECT, ERR_SOURCE_LackingSubmitterSeqids, "One ore more source features are lacking /submitter_seqid qualifiers provided in others. Entry dropped.");
    return (false);
}

/**********************************************************/
void ParseSourceFeat(ParserPtr pp, DataBlkCIter dbp, DataBlkCIter dbp_end, const CSeq_id& seqid, Int2 type, 
        const CBioseq& bioseq, TSeqFeatList& seq_feats)
{
    SourceFeatBlkPtr sfbp;
    SourceFeatBlkPtr tsfbp;

    MinMaxPtr   mmp;
    IndexblkPtr ibp;
    char*       res;
    char*       acc;
    Int4        i;
    Int4        use_what = USE_ALL;
    bool        err;
    ErrSev      sev;
    bool        need_focus;
    bool        already;

    ibp        = pp->entrylist[pp->curindx];
    acc        = ibp->acnum;
    size_t len = ibp->bases;

    pp->errstat = 0;

    sfbp = CollectSourceFeats(dbp, dbp_end, type);
    if (! sfbp) {
        FtaErrPost(SEV_REJECT, ERR_SOURCE_FeatureMissing, "Required source feature is missing. Entry dropped.");
        return;
    }

    RemoveSourceFeatSpaces(sfbp);
    CheckForExemption(sfbp);

    if (! CheckSourceFeatLocFuzz(sfbp)) {
        SourceFeatBlkSetFree(sfbp);
        return;
    }

    res = CheckSourceFeatLocAccs(sfbp, acc);
    if (res) {
        FtaErrPost(SEV_REJECT, ERR_SOURCE_BadLocation, "Source feature location points to another record: \"{}\". Entry dropped.", res);
        SourceFeatBlkSetFree(sfbp);
        return;
    }

    if (! SourceFeatStructFillIn(ibp, sfbp, use_what)) {
        FtaErrPost(SEV_REJECT, ERR_SOURCE_MultipleMolTypes, "Multiple /mol_type qualifiers were encountered within source feature. Entry dropped.");
        SourceFeatBlkSetFree(sfbp);
        return;
    }

    if (! ibp->submitter_seqid.empty() && ! CheckSubmitterSeqidQuals(sfbp, acc)) {
        ibp->submitter_seqid.clear();
        SourceFeatBlkSetFree(sfbp);
        return;
    }

    if (! CheckMoltypeConsistency(sfbp, ibp->moltype)) {
        FtaErrPost(SEV_REJECT, ERR_SOURCE_InconsistentMolType, "Inconsistent /mol_type qualifiers were encountered. Entry dropped.");
        SourceFeatBlkSetFree(sfbp);
        return;
    }

    res = CheckSourceFeatFocusAndTransposon(sfbp);
    if (res) {
        FtaErrPost(SEV_REJECT, ERR_SOURCE_FocusAndTransposonNotAllowed, "/transposon (or /insertion_seq) qualifiers should not be used in conjunction with /focus. Source feature at \"{}\". Entry dropped.", res);
        SourceFeatBlkSetFree(sfbp);
        return;
    }

    res = CheckSourceFeatOrgs(sfbp, &i);
    if (res) {
        if (i == 1) {
            FtaErrPost(SEV_REJECT, ERR_SOURCE_NoOrganismQual, "/organism qualifier contains only organell/genome name. No genus/species present. Source feature at \"{}\". Entry dropped.", res);
        } else {
            FtaErrPost(SEV_REJECT, ERR_SOURCE_OrganismIncomplete, "Required /organism qualifier is containing genome info only at \"{}\". Entry dropped.", res);
        }
        SourceFeatBlkSetFree(sfbp);
        return;
    }

    CompareDescrFeatSources(sfbp, bioseq);

    CreateRawBioSources(pp, sfbp, use_what);

    if (! CheckSourceLineage(sfbp, pp->source, ibp->is_pat)) {
        SourceFeatBlkSetFree(sfbp);
        return;
    }

    PropagateSuppliedLineage(bioseq, sfbp, pp->taxserver);

    mmp = new MinMax;
    i   = CheckSourceFeatCoverage(sfbp, mmp, len);
    if (i != 0) {
        if (i == 1) {
            FtaErrPost(SEV_REJECT, ERR_SOURCE_IncompleteCoverage, "Supplied source features do not span every base of the sequence. Entry dropped.");
        } else {
            FtaErrPost(SEV_REJECT, ERR_SOURCE_ExcessCoverage, "Sequence is spanned by too many source features. Entry dropped.");
        }
        SourceFeatBlkSetFree(sfbp);
        MinMaxFree(mmp);
        return;
    }

    if (! CheckForENV(sfbp, ibp, pp->source)) {
        SourceFeatBlkSetFree(sfbp);
        MinMaxFree(mmp);
        return;
    }

    if (! CheckSYNTGNDivision(sfbp, ibp->division)) {
        SourceFeatBlkSetFree(sfbp);
        MinMaxFree(mmp);
        return;
    }

    if (pp->source == Parser::ESource::EMBL)
        need_focus = CheckNeedSYNFocus(sfbp);
    else
        need_focus = true;

    already = false;
    i       = CheckTransgenicSourceFeats(sfbp);
    if (i == 5) {
        if (pp->source == Parser::ESource::DDBJ || pp->source == Parser::ESource::EMBL)
            sev = SEV_WARNING;
        else
            sev = SEV_ERROR;
        FtaErrPost(sev, ERR_SOURCE_TransSingleOrgName, "Use of /transgenic requires at least two source features with differences among /organism, /strain, /organelle, and /isolate, between the host and foreign organisms.");
    } else if (i > 0) {
        sev = SEV_REJECT;
        if (i == 1) {
            FtaErrPost(sev, ERR_SOURCE_TransgenicTooShort, "Source feature with /transgenic qualifier does not span the entire sequence. Entry dropped.");
        } else if (i == 2) {
            FtaErrPost(sev, ERR_SOURCE_FocusAndTransgenicQuals, "Both /focus and /transgenic qualifiers exist; these quals are mutually exclusive. Entry dropped.");
        } else if (i == 3) {
            FtaErrPost(sev, ERR_SOURCE_MultipleTransgenicQuals, "Multiple source features have /transgenic qualifiers. Entry dropped.");
        } else {
            already = true;
            if (! need_focus)
                sev = SEV_ERROR;
            FtaErrPost(sev, ERR_SOURCE_FocusQualMissing, "Multiple organism names exist, but no source feature has a /focus qualifier.{}", (sev == SEV_ERROR) ? "" : " Entry dropped.");
        }

        if (sev == SEV_REJECT) {
            SourceFeatBlkSetFree(sfbp);
            MinMaxFree(mmp);
            return;
        }
    }

    res = CheckWholeSourcesVersusFocused(sfbp);
    if (res) {
        FtaErrPost(SEV_REJECT, ERR_SOURCE_FocusQualNotFullLength, "/focus qualifier should be used for the full-length source feature, not on source feature at \"{}\".", res);
        SourceFeatBlkSetFree(sfbp);
        MinMaxFree(mmp);
        return;
    }
    i = CheckFocusInOrgs(sfbp, len, &pp->errstat);
    if (pp->errstat != 0)
        i = 1;
    if (i > 0) {
        sev = SEV_REJECT;
        if (i == 1) {
            FtaErrPost(sev, ERR_SOURCE_FocusQualNotNeeded, "/focus qualifier present, but only one organism name exists. Entry dropped.");
        } else if (i == 2) {
            FtaErrPost(sev, ERR_SOURCE_MultipleOrganismWithFocus, "/focus qualifiers exist on source features with differing organism names. Entry dropped.");
        } else {
            if (! need_focus)
                sev = SEV_ERROR;
            if (! already)
                FtaErrPost(sev, ERR_SOURCE_FocusQualMissing, "Multiple organism names exist, but no source feature has a /focus qualifier.{}", (sev == SEV_ERROR) ? "" : " Entry dropped.");
        }

        if (sev == SEV_REJECT) {
            SourceFeatBlkSetFree(sfbp);
            MinMaxFree(mmp);
            return;
        }
    }
    res = CheckSourceOverlap(mmp->next, len);
    MinMaxFree(mmp);
    if (res) {
        FtaErrPost(SEV_REJECT, ERR_SOURCE_MultiOrgOverlap, "Overlapping source features have different organism names {}. Entry dropped.", res);
        SourceFeatBlkSetFree(sfbp);
        MemFree(res);
        return;
    }

    res = CheckForUnusualFullLengthOrgs(sfbp);
    if (res) {
        FtaErrPost(SEV_WARNING, ERR_SOURCE_UnusualOrgName, "Unusual organism name \"{}\" encountered for full-length source feature.", res);
    }

    for (tsfbp = sfbp, i = 0; tsfbp; tsfbp = tsfbp->next)
        i++;
    if (i > BIOSOURCES_THRESHOLD) {
        FtaErrPost(SEV_WARNING, ERR_SOURCE_ManySourceFeats, "This record has more than {} source features.", BIOSOURCES_THRESHOLD);
    }

    if (! ParsePcrPrimers(sfbp)) {
        SourceFeatBlkSetFree(sfbp);
        return;
    }

    CheckCollectionDate(sfbp, pp->source);

    sfbp = PickTheDescrSource(sfbp);
    if (! sfbp || ! UpdateRawBioSource(sfbp, pp->source, ibp, pp->taxserver)) {
        SourceFeatBlkSetFree(sfbp);
        return;
    }
    if(sfbp->lookup == false)
        ibp->biodrop = true;

    if (sfbp->focus)
        sfbp->bio_src->SetIs_focus();
    else
        sfbp->bio_src->ResetIs_focus();


    for (tsfbp = sfbp; tsfbp; tsfbp = tsfbp->next) {
        CheckMetagenome(*tsfbp->bio_src);

        CRef<CSeq_feat> feat(new CSeq_feat);
        feat->SetData().SetBiosrc(*tsfbp->bio_src);

        pp->buf.reset();

        GetSeqLocation(*feat, tsfbp->location, seqid, &err, pp, "source");

        if (err) {
            FtaErrPost(SEV_ERROR, ERR_FEATURE_Dropped, "/source|{}| range check detects problems. Entry dropped.", tsfbp->location);
            break;
        }

        if (! tsfbp->quals.empty()) {
            auto p = GetTheQualValue(tsfbp->quals, "evidence");
            if (p) {
                if (NStr::CompareNocase(p->c_str(), "experimental") == 0)
                    feat->SetExp_ev(CSeq_feat::eExp_ev_experimental);
                else if (NStr::CompareNocase(p->c_str(), "not_experimental") == 0)
                    feat->SetExp_ev(CSeq_feat::eExp_ev_not_experimental);
            }
        }

        seq_feats.push_back(feat);
    }

    SourceFeatBlkSetFree(sfbp);

    if (tsfbp)
        seq_feats.clear();
}

END_NCBI_SCOPE
