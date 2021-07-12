/* fta_src.c
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
 * File Name:  fta_src.c
 *
 * Author: Sergey Bazhin
 *
 * File Description:
 * -----------------
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


typedef struct {
    const char *name;
    Uint1      num;
} CharUInt1;

#define USE_CULTIVAR                         00001
#define USE_ISOLATE                          00002
#define USE_SEROTYPE                         00004
#define USE_SEROVAR                          00010
#define USE_SPECIMEN_VOUCHER                 00020
#define USE_STRAIN                           00040
#define USE_SUB_SPECIES                      00100
#define USE_SUB_STRAIN                       00200
#define USE_VARIETY                          00400
#define USE_ECOTYPE                          01000
#define USE_ALL                              01777

#define BIOSOURCES_THRESHOLD                 20

typedef struct _pcr_primers {
    char*                  fwd_name;
    char*                  fwd_seq;
    char*                  rev_name;
    char*                  rev_seq;
    struct _pcr_primers* next;
} PcrPrimers, *PcrPrimersPtr;

typedef struct _source_feat_blk {
    char*                      name;
    char*                      strain;
    char*                      organelle;
    char*                      isolate;
    char*                      namstr;
    char*                      location;
    char*                      moltype;
    char*                      genomename;
    char*                      submitter_seqid;

    TQualVector quals;
    CRef<objects::CBioSource> bio_src;
    CRef<objects::COrgName> orgname;

    bool                      full;
    bool                      focus;
    bool                      tg;
    bool                      lookup;
    bool                      skip;
    bool                      useit;

    Uint1                        genome;
    struct _source_feat_blk* next;

    _source_feat_blk() :
        name(NULL),
        strain(NULL),
        organelle(NULL),
        isolate(NULL),
        namstr(NULL),
        location(NULL),
        moltype(NULL),
        genomename(NULL),
        submitter_seqid(NULL),
        full(false),
        focus(false),
        tg(false),
        lookup(false),
        skip(false),
        useit(false),
        genome(0),
        next(NULL)
    {}

} SourceFeatBlk, *SourceFeatBlkPtr;

typedef struct _min_max {
    char*              orgname;       /* Do not free! It's just a pointer */
    Int4                 min;
    Int4                 max;
    bool                 skip;
    struct _min_max* next;
} MinMax, *MinMaxPtr;

static const char *ObsoleteSourceDbxrefTag[] = {
    "IFO",
    NULL
};

static const char *DENLRSourceDbxrefTag[] = {   /* DENL = DDBJ + EMBL + NCBI +
                                                          LANL + RefSeq */
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
    NULL
};

static const char *DESourceDbxrefTag[] = {      /* DE = DDBJ + EMBL */
    "FANTOM_DB",
    "IMGT/HLA",
    "IMGT/LIGM",
    "MGD",
    "MGI",
    NULL
};

static const char *ESourceDbxrefTag[] = {       /* E = EMBL */
    "UNITE",
    NULL
};

static const char *NLRSourceDbxrefTag[] = {     /* N = NCBI + LANL + RefSeq */
    "FLYBASE",
    NULL
};

static const char *exempt_quals[] = {
    "transposon",
    "insertion_seq",
    NULL
};

static const char *special_orgs[] = {
    "synthetic construct",
    "artificial sequence",
    "eukaryotic synthetic construct",
    NULL
};

static const char *unusual_toks[] = {
    "complement",
    NULL
};

static const char *source_genomes[] = {
    "mitochondr",
    "chloroplast",
    "kinetoplas",
    "cyanelle",
    "plastid",
    "chromoplast",
    "macronuclear",
    "extrachrom",
    "plasmid",
    NULL
};

static const char *SourceBadQuals[] = {
    "label",
    "usedin",
    "citation",
    NULL
};

static const char *SourceSubSources[] = {
    "chromosome",                       /*  1 */
    "map",                              /*  2 */
    "clone",                            /*  3 */
    "sub_clone",                        /*  4 */
    "haplotype",                        /*  5 */
    "genotype",                         /*  6 */
    "sex",                              /*  7 */
    "cell_line",                        /*  8 */
    "cell_type",                        /*  9 */
    "tissue_type",                      /* 10 */
    "clone_lib",                        /* 11 */
    "dev_stage",                        /* 12 */
    "frequency",                        /* 13 */
    "germline",                         /* 14 */
    "rearranged",                       /* 15 */
    "lab_host",                         /* 16 */
    "pop_variant",                      /* 17 */
    "tissue_lib",                       /* 18 */
    "plasmid",                          /* 19 */
    "transposon",                       /* 20 */
    "insertion_seq",                    /* 21 */
    "plastid",                          /* 22 */
    "",                                 /* 23 */
    "segment",                          /* 24 */
    "",                                 /* 25 */
    "transgenic",                       /* 26 */
    "environmental_sample",             /* 27 */
    "isolation_source",                 /* 28 */
    "lat_lon",                          /* 29 */
    "collection_date",                  /* 30 */
    "collected_by",                     /* 31 */
    "identified_by",                    /* 32 */
    "",                                 /* 33 */
    "",                                 /* 34 */
    "",                                 /* 35 */
    "",                                 /* 36 */
    "metagenomic",                      /* 37 */
    "mating_type",                      /* 38 */
    NULL
};

static CharUInt1 SourceOrgMods[] = {
    {"strain",              2},
    {"sub_strain",          3},
    {"variety",             6},
    {"serotype",            7},
    {"serovar",             9},
    {"cultivar",           10},
    {"isolate",            17},
    {"specific_host",      21},
    {"host",               21},
    {"sub_species",        22},
    {"specimen_voucher",   23},
    {"ecotype",            27},
    {"culture_collection", 35},
    {"bio_material",       36},
    {"metagenome_source",  37},
    {"type_material",      38},
    {NULL,                  0}
};

static const char *GenomicSourceFeatQual[] = {
    "unknown",
    "unknown",
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
    "proplastid",                       /* 18 */
    "",                                 /* 19 */
    "",                                 /* 20 */
    "",                                 /* 21 */
    "chromatophore",                    /* 22 */
    NULL
};

static const char *OrganelleFirstToken[] = {
    "chromatophore",
    "hydrogenosome",
    "mitochondrion",
    "nucleomorph",
    "plastid",
    NULL
};

/**********************************************************/
static SourceFeatBlkPtr SourceFeatBlkNew(void)
{
    return new SourceFeatBlk;
}

/**********************************************************/
static void SourceFeatBlkFree(SourceFeatBlkPtr sfbp)
{
    if (sfbp->name != NULL)
        MemFree(sfbp->name);
    if(sfbp->strain != NULL)
        MemFree(sfbp->strain);
    if(sfbp->organelle != NULL)
        MemFree(sfbp->organelle);
    if(sfbp->isolate != NULL)
        MemFree(sfbp->isolate);
    if(sfbp->namstr != NULL)
        MemFree(sfbp->namstr);
    if(sfbp->location != NULL)
        MemFree(sfbp->location);
    if(sfbp->moltype != NULL)
        MemFree(sfbp->moltype);
    if(sfbp->genomename != NULL)
        MemFree(sfbp->genomename);
    
    delete sfbp;
}

/**********************************************************/
static void SourceFeatBlkSetFree(SourceFeatBlkPtr sfbp)
{
    SourceFeatBlkPtr tsfbp;

    for(tsfbp = sfbp; tsfbp != NULL; tsfbp = sfbp)
    {
        sfbp = tsfbp->next;
        SourceFeatBlkFree(tsfbp);
    }
}

/**********************************************************/
static SourceFeatBlkPtr CollectSourceFeats(DataBlkPtr dbp, Int2 type)
{
    SourceFeatBlkPtr sfbp;
    SourceFeatBlkPtr tsfbp;
    DataBlkPtr       tdbp;
    FeatBlkPtr       fbp;

    sfbp = SourceFeatBlkNew();
    tsfbp = sfbp;

    for(; dbp != NULL; dbp = dbp->next)
    {
        if(dbp->type != type)
            continue;
        for(tdbp = (DataBlkPtr) dbp->data; tdbp != NULL; tdbp = tdbp->next)
        {
            fbp = (FeatBlkPtr) tdbp->data;
            if(fbp == NULL || fbp->key == NULL ||
               StringCmp(fbp->key, "source") != 0)
                continue;
            tsfbp->next = SourceFeatBlkNew();
            tsfbp = tsfbp->next;
            if(fbp->location != NULL)
                tsfbp->location = StringSave(fbp->location);
            tsfbp->quals = fbp->quals;
        }
    }
    tsfbp = sfbp->next;
    delete sfbp;
    //MemFree(sfbp);
    return(tsfbp);
}

/**********************************************************/
static void RemoveStringSpaces(char* line)
{
    char* p;
    char* q;

    if(line == NULL || *line == '\0')
        return;

    for(p = line, q = line; *p != '\0'; p++)
        if(*p != ' ' && *p != '\t')
            *q++ = *p;
    *q = '\0';
}

/**********************************************************/
static void RemoveSourceFeatSpaces(SourceFeatBlkPtr sfbp)
{
    for(; sfbp != NULL; sfbp = sfbp->next)
    {
        RemoveStringSpaces(sfbp->location);
        NON_CONST_ITERATE(TQualVector, cur, sfbp->quals)
        {
            if ((*cur)->IsSetQual())
            {
                std::vector<char> buf((*cur)->GetQual().begin(), (*cur)->GetQual().end());
                buf.push_back(0);
                ShrinkSpaces(&buf[0]);
                (*cur)->SetQual(&buf[0]);
            }

            if ((*cur)->IsSetVal())
            {
                std::vector<char> buf((*cur)->GetVal().begin(), (*cur)->GetVal().end());
                buf.push_back(0);
                ShrinkSpaces(&buf[0]);
                (*cur)->SetVal(&buf[0]);
            }
        }
    }
}

/**********************************************************/
static void CheckForExemption(SourceFeatBlkPtr sfbp)
{
    const char   **b;

    for(; sfbp != NULL; sfbp = sfbp->next)
    {
        ITERATE(TQualVector, cur, sfbp->quals)
        {
            for (b = exempt_quals; *b != NULL; b++)
            {
                if ((*cur)->GetQual() == *b)
                    break;
            }
            if(*b != NULL)
            {
                sfbp->skip = true;
                break;
            }
        }
    }
}

/**********************************************************/
static void PopulateSubNames(char* namstr, const Char *name,
                             const Char* value, Uint1 subtype, TOrgModList& mods)
{
    CRef<objects::COrgMod> mod(new objects::COrgMod);

    StringCat(namstr, name);
    StringCat(namstr, value);
    StringCat(namstr, ")");

    mod->SetSubtype(subtype);
    mod->SetSubname(value);

    mods.push_front(mod);
}

/**********************************************************/
static void CollectSubNames(SourceFeatBlkPtr sfbp, Int4 use_what, const Char* name,
                            const Char* cultivar, const Char* isolate,
                            const Char* serotype, const Char* serovar,
                            const Char* specimen_voucher, const Char* strain,
                            const Char* sub_species, const Char* sub_strain,
                            const Char* variety, const Char* ecotype)
{
    if(sfbp == NULL)
       return;

    if(sfbp->namstr != NULL)
        MemFree(sfbp->namstr);
    sfbp->namstr = NULL;

    if (sfbp->orgname.NotEmpty())
        sfbp->orgname.Reset();

    if(name == NULL)
       return;

    size_t i = StringLen(name) + 1;
    size_t j = i;
    if((use_what & USE_CULTIVAR) == USE_CULTIVAR && cultivar != NULL)
        i += (StringLen(cultivar) + StringLen("cultivar") + 5);
    if((use_what & USE_ISOLATE) == USE_ISOLATE && isolate != NULL)
        i += (StringLen(isolate) + StringLen("isolate") + 5);
    if((use_what & USE_SEROTYPE) == USE_SEROTYPE && serotype != NULL)
        i += (StringLen(serotype) + StringLen("serotype") + 5);
    if((use_what & USE_SEROVAR) == USE_SEROVAR && serovar != NULL)
        i += (StringLen(serovar) + StringLen("serovar") + 5);
    if((use_what & USE_SPECIMEN_VOUCHER) == USE_SPECIMEN_VOUCHER &&
       specimen_voucher != NULL)
        i += (StringLen(specimen_voucher) + StringLen("specimen_voucher") + 5);
    if((use_what & USE_STRAIN) == USE_STRAIN && strain != NULL)
        i += (StringLen(strain) + StringLen("strain") + 5);
    if((use_what & USE_SUB_SPECIES) == USE_SUB_SPECIES && sub_species != NULL)
        i += (StringLen(sub_species) + StringLen("sub_species") + 5);
    if((use_what & USE_SUB_STRAIN) == USE_SUB_STRAIN && sub_strain != NULL)
        i += (StringLen(sub_strain) + StringLen("sub_strain") + 5);
    if((use_what & USE_VARIETY) == USE_VARIETY && variety != NULL)
        i += (StringLen(variety) + StringLen("variety") + 5);
    if((use_what & USE_ECOTYPE) == USE_ECOTYPE && ecotype != NULL)
        i += (StringLen(ecotype) + StringLen("ecotype") + 5);
    sfbp->namstr = (char*) MemNew(i);
    StringCpy(sfbp->namstr, name);
    if(i == j)
        return;

    sfbp->orgname = new objects::COrgName;
    TOrgModList& mods = sfbp->orgname->SetMod();

    if((use_what & USE_CULTIVAR) == USE_CULTIVAR && cultivar != NULL)
        PopulateSubNames(sfbp->namstr, "  (cultivar ", cultivar, 10, mods);
    if((use_what & USE_ISOLATE) == USE_ISOLATE && isolate != NULL)
        PopulateSubNames(sfbp->namstr, "  (isolate ", isolate, 17, mods);
    if((use_what & USE_SEROTYPE) == USE_SEROTYPE && serotype != NULL)
        PopulateSubNames(sfbp->namstr, "  (serotype ", serotype, 7, mods);
    if((use_what & USE_SEROVAR) == USE_SEROVAR && serovar != NULL)
        PopulateSubNames(sfbp->namstr, "  (serovar ", serovar, 9, mods);
    if((use_what & USE_SPECIMEN_VOUCHER) == USE_SPECIMEN_VOUCHER &&
       specimen_voucher != NULL)
        PopulateSubNames(sfbp->namstr, "  (specimen_voucher ", specimen_voucher, 23, mods);
    if((use_what & USE_STRAIN) == USE_STRAIN && strain != NULL)
        PopulateSubNames(sfbp->namstr, "  (strain ", strain, 2, mods);
    if((use_what & USE_SUB_SPECIES) == USE_SUB_SPECIES && sub_species != NULL)
        PopulateSubNames(sfbp->namstr, "  (sub_species ", sub_species, 22, mods);
    if((use_what & USE_SUB_STRAIN) == USE_SUB_STRAIN && sub_strain != NULL)
        PopulateSubNames(sfbp->namstr, "  (sub_strain ", sub_strain, 3, mods);
    if((use_what & USE_VARIETY) == USE_VARIETY && variety != NULL)
        PopulateSubNames(sfbp->namstr, "  (variety ", variety, 6, mods);
    if((use_what & USE_ECOTYPE) == USE_ECOTYPE && ecotype != NULL)
        PopulateSubNames(sfbp->namstr, "  (ecotype ", ecotype, 27, mods);
}

/**********************************************************/
static bool SourceFeatStructFillIn(IndexblkPtr ibp, SourceFeatBlkPtr sfbp, Int4 use_what)
{
    const Char **b;

    const Char*    name;
    const Char*    cultivar;
    const Char*    isolate;
    const Char*    organelle;
    const Char*    serotype;
    const Char*    serovar;
    const Char*    ecotype;
    const Char*    specimen_voucher;
    const Char*    strain;
    const Char*    sub_species;
    const Char*    sub_strain;
    const Char*    variety;
    char*    genomename;
    const Char*    p;
    char*    q;
    bool       ret;
    Int4       i;

    for(ret = true; sfbp != NULL; sfbp = sfbp->next)
    {
        name = NULL;
        cultivar = NULL;
        isolate = NULL;
        organelle = NULL;
        serotype = NULL;
        serovar = NULL;
        ecotype = NULL;
        specimen_voucher = NULL;
        strain = NULL;
        sub_species = NULL;
        sub_strain = NULL;
        variety = NULL;
        genomename = NULL;

        ITERATE(TQualVector, cur, sfbp->quals)
        {
            if (!(*cur)->IsSetQual())
                continue;

            const std::string& qual_str = (*cur)->GetQual();
            const Char* val_ptr = (*cur)->IsSetVal() ? (*cur)->GetVal().c_str() : NULL;

            if (qual_str == "db_xref")
            {
                q = StringChr(val_ptr, ':');
                if(q == NULL || q[1] == '\0')
                    continue;
                *q = '\0';
                if (StringICmp(val_ptr, "taxon") == 0)
                    if(ibp->taxid < 1)
                        ibp->taxid = atoi(q + 1);
                *q = ':';
                continue;
            }
            if (qual_str == "focus")
            {
                sfbp->focus = true;
                continue;
            }
            if (qual_str == "transgenic")
            {
                sfbp->tg = true;
                continue;
            }
            if (qual_str == "cultivar")
            {
                cultivar = val_ptr;
                continue;
            }
            if (qual_str == "isolate")
            {
                if(isolate == NULL)
                    isolate = val_ptr;
                continue;
            }
            if (qual_str == "mol_type")
            {
                if(sfbp->moltype != NULL)
                    ret = false;
                else if (val_ptr != NULL)
                    sfbp->moltype = StringSave(val_ptr);
                continue;
            }
            if (qual_str == "organelle")
            {
                if(organelle == NULL)
                    organelle = val_ptr;
                continue;
            }
            if (qual_str == "serotype")
            {
                serotype = val_ptr;
                continue;
            }
            if (qual_str == "serovar")
            {
                serovar = val_ptr;
                continue;
            }
            if (qual_str == "ecotype")
            {
                ecotype = val_ptr;
                continue;
            }
            if (qual_str == "specimen_voucher")
            {
                specimen_voucher = val_ptr;
                continue;
            }
            if (qual_str == "strain")
            {
                if(strain == NULL)
                    strain = val_ptr;
                continue;
            }
            if (qual_str == "sub_species")
            {
                sub_species = val_ptr;
                continue;
            }
            if (qual_str == "sub_strain")
            {
                sub_strain = val_ptr;
                continue;
            }
            if (qual_str == "variety")
            {
                variety = val_ptr;
                continue;
            }
            if(qual_str == "submitter_seqid")
            {
                if(sfbp->submitter_seqid != NULL)
                {
                    MemFree(sfbp->submitter_seqid);
                    sfbp->submitter_seqid = StringSave("");
                }
                else
                    sfbp->submitter_seqid = StringSave(val_ptr);
                if(ibp->submitter_seqid == NULL)
                    ibp->submitter_seqid = StringSave(val_ptr);
                continue;
            }

            if (qual_str != "organism" ||
                val_ptr == NULL || val_ptr[0] == '\0')
                continue;

            if(ibp->organism == NULL)
                ibp->organism = StringSave(val_ptr);

            p = StringChr(val_ptr, ' ');

            std::string str_to_find;
            if (p != NULL)
                str_to_find.assign(val_ptr, p);
            else
                str_to_find.assign(val_ptr);

            for(i = 0, b = source_genomes; *b != NULL; b++, i++)
                if (StringNICmp(str_to_find.c_str(), *b, StringLen(*b)) == 0)
                    break;
            if(*b != NULL && i != 8)
            {
                if(genomename != NULL)
                    MemFree(genomename);
                genomename = StringSave(str_to_find.c_str());
            }

            if(p != NULL)
                ++p;

            if(*b == NULL)
                p = val_ptr;
            else
            {
                if(i == 0)
                    sfbp->genome = 5;   /* Mitochondrion */
                else if(i == 1)
                    sfbp->genome = 2;   /* Chloroplast */
                else if(i == 2)
                    sfbp->genome = 4;   /* Kinetoplast */
                else if(i == 3)
                    sfbp->genome = 12;  /* Cyanelle */
                else if(i == 4)
                    sfbp->genome = 6;   /* Plastid */
                else if(i == 5)
                    sfbp->genome = 3;   /* Chromoplast */
                else if(i == 6)
                    sfbp->genome = 7;   /* Macronuclear */
                else if(i == 7)
                    sfbp->genome = 8;   /* Extrachrom */
                else if(i == 8)
                {
                    p = val_ptr;
                    sfbp->genome = 9;   /* Plasmid */
                }
            }
            name = p;
        }

        if(sfbp->name != NULL)
            MemFree(sfbp->name);
        sfbp->name = (name == NULL) ? NULL : StringSave(name);

        if(sfbp->genomename != NULL)
            MemFree(sfbp->genomename);
        sfbp->genomename = genomename;

        if(strain != NULL && sfbp->strain == NULL)
            sfbp->strain = StringSave(strain);
        if(isolate != NULL && sfbp->isolate == NULL)
            sfbp->isolate = StringSave(isolate);
        if(organelle != NULL && sfbp->organelle == NULL)
            sfbp->organelle = StringSave(organelle);

        CollectSubNames(sfbp, use_what, name, cultivar, isolate, serotype,
                        serovar, specimen_voucher, strain, sub_species,
                        sub_strain, variety, ecotype);
    }
    return(ret);
}

/**********************************************************/
static char* CheckSourceFeatFocusAndTransposon(SourceFeatBlkPtr sfbp)
{
    for (; sfbp != NULL; sfbp = sfbp->next)
    {
        if (sfbp->focus && sfbp->skip)
            break;
    }

    if(sfbp != NULL)
        return(sfbp->location);
    return(NULL);
}

/**********************************************************/
static char* CheckSourceFeatOrgs(SourceFeatBlkPtr sfbp, int* status)
{
    *status = 0;
    for(; sfbp != NULL; sfbp = sfbp->next)
    {
/**        if(sfbp->namstr != NULL)*/
        if(sfbp->name != NULL)
            continue;

        *status = (sfbp->genome == 0) ? 1 : 2;
        break;
    }
    if(sfbp != NULL)
        return(sfbp->location);
    return(NULL);
}

/**********************************************************/
static bool CheckSourceFeatLocFuzz(SourceFeatBlkPtr sfbp)
{
    const char **b;
    char*    p;
    char*    q;
    Int4       count;
    bool    partial;
    bool    invalid;
    bool    ret;

    ret = true;
    for(; sfbp != NULL; sfbp = sfbp->next)
    {
        if(sfbp->location == NULL || sfbp->location[0] == '\0')
            break;
        if(sfbp->skip)
            continue;

        ITERATE(TQualVector, cur, sfbp->quals)
        {
            if ((*cur)->GetQual() != "partial")
                continue;

            ErrPostEx(SEV_ERROR, ERR_SOURCE_PartialQualifier,
                      "Source feature location has /partial qualifier. Qualifier has been ignored: \"%s\".",
                      (sfbp->location == NULL) ? "?empty?" : sfbp->location);
            break;
        }

        for(b = unusual_toks; *b != NULL; b++)
        {
            p = StringStr(sfbp->location, *b);
            if(p == NULL)
                continue;
            q = p + StringLen(*b);
            if(p > sfbp->location)
                p--;
            if((p == sfbp->location || *p == '(' || *p == ')' ||
                *p == ':' || *p == ',' || *p == '.') &&
               (*q == '\0' || *q == '(' || *q == ')' || *q == ',' ||
                *q == ':' || *q == '.'))
            {
                ErrPostEx(SEV_ERROR, ERR_SOURCE_UnusualLocation,
                          "Source feature has an unusual location: \"%s\".",
                          (sfbp->location == NULL) ? "?empty?" : sfbp->location);
                break;
            }
        }

        partial = false;
        invalid = false;
        for(count = 0, p = sfbp->location; *p != '\0'; p++)
        {
            if(*p == '^')
                invalid = true;
            else if(*p == '>' || *p == '<')
                partial = true;
            else if(*p == '(')
                count++;
            else if(*p == ')')
                count--;
            else if(*p == '.' && p[1] == '.')
                p++;
            else if(*p == '.' && p[1] != '.')
            {
                for(q = p + 1; *q >= '0' && *q <= '9';)
                    q++;
                if(q == p || *q != ':')
                    invalid = true;
            }
        }
        if(partial)
        {
            ErrPostEx(SEV_ERROR, ERR_SOURCE_PartialLocation,
                      "Source feature location is partial; partiality flags have been ignored: \"%s\".",
                      (sfbp->location == NULL) ? "?empty?" : sfbp->location);
        }
        if(invalid || count != 0)
        {
            ErrPostEx(SEV_REJECT, ERR_SOURCE_InvalidLocation,
                      "Invalid location for source feature at \"%s\". Entry dropped.",
                      (sfbp->location == NULL) ? "?empty?" : sfbp->location);
            ret = false;
        }
    }
    return(ret);
}

/**********************************************************/
static char* CheckSourceFeatLocAccs(SourceFeatBlkPtr sfbp, char* acc)
{
    char* p;
    char* q;
    char* r;
    Int4    i;

    for(; sfbp != NULL; sfbp = sfbp->next)
    {
        if(sfbp->location == NULL || sfbp->location[0] == '\0')
            continue;
        for(p = sfbp->location + 1; *p != '\0'; p++)
        {
            if(*p != ':')
                continue;
            for(r = NULL, q = p - 1;; q--)
            {
                if(q == sfbp->location)
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
            i = StringICmp(q, acc);
            if(r != NULL)
                *r = '.';
            else
                *p = ':';
            if(i != 0)
                break;
        }
        if(*p != '\0')
            break;
    }
    if(sfbp == NULL)
        return(NULL);
    return(sfbp->location);
}

/**********************************************************/
static void MinMaxFree(MinMaxPtr mmp)
{
    MinMaxPtr tmmp;

    for(; mmp != NULL; mmp = tmmp)
    {
        tmmp = mmp->next;
        MemFree(mmp);
    }
}

/**********************************************************/
bool fta_if_special_org(const Char* name)
{
    const char **b;

    if(name == NULL || *name == '\0')
        return false;

    for(b = special_orgs; *b != NULL; b++)
        if(StringICmp(*b, name) == 0)
            break;
    if(*b != NULL || StringIStr(name, "vector") != NULL)
        return true;
    return false;
}

/**********************************************************/
static Int4 CheckSourceFeatCoverage(SourceFeatBlkPtr sfbp, MinMaxPtr mmp,
                                    size_t len)
{
    SourceFeatBlkPtr tsfbp;
    MinMaxPtr        tmmp;
    MinMaxPtr        mmpnext;
    char*          p;
    char*          q;
    char*          r;
    char*          loc;
    Int4             count;
    Int4             min;
    Int4             max;
    Int4             i;
    Int4             tgs;
    Int4             sporg;

    loc = NULL;
    tmmp = mmp;
    for(tsfbp = sfbp; tsfbp != NULL; tsfbp = tsfbp->next)
    {
        if(tsfbp->location == NULL || tsfbp->location[0] == '\0' ||
           tsfbp->name == NULL || tsfbp->name[0] == '\0')
            continue;
        if(loc != NULL)
            MemFree(loc);
        loc = StringSave(tsfbp->location);
        for(p = loc; *p != '\0'; p++)
            if(*p == ',' || *p == '(' || *p == ')' || *p == ':' ||
               *p == ';' || *p == '^')
                *p = ' ';
        for(p = loc, q = loc; *p != '\0';)
        {
            if(*p == '>' || *p == '<')
            {
                p++;
                continue;
            }
            *q++ = *p;
            if(*p == ' ')
                while(*p == ' ')
                    p++;
            else
                p++;
        }
        if(q > loc && *(q - 1) == ' ')
            q--;
        *q = '\0';

        q = (*loc == ' ') ? (loc + 1) : loc;
        for(p = q;;)
        {
            min = 0;
            max = 0;
            p = StringChr(p, ' ');
            if(p != NULL)
                *p++ = '\0';
            for(r = q; *r >= '0' && *r <= '9';)
                r++;
            if(*r == '\0')
            {
                i = atoi(q);
                if(i > 0)
                {
                    min = i;
                    max = i;
                }
            }
            else if(*r == '.' && r[1] == '.')
            {
                *r++ = '\0';
                min = atoi(q);
                if(min > 0)
                {
                    for(q = ++r; *r >= '0' && *r <= '9';)
                        r++;
                    if(*r == '\0')
                        max = atoi(q);
                }
            }
            if(min > 0 && max > 0)
            {
                if(min == 1 && (size_t) max == len)
                    tsfbp->full = true;
                for(tmmp = mmp;; tmmp = tmmp->next)
                {
                    if(min < tmmp->min)
                    {
                        mmpnext = tmmp->next;
                        tmmp->next = (MinMaxPtr) MemNew(sizeof(MinMax));
                        tmmp->next->orgname = tmmp->orgname;
                        tmmp->next->min = tmmp->min;
                        tmmp->next->max = tmmp->max;
                        tmmp->next->skip = tmmp->skip;
                        tmmp->next->next = mmpnext;
                        tmmp->orgname = tsfbp->name;
                        tmmp->min = min;
                        tmmp->max = max;
                        tmmp->skip = tsfbp->skip;
                        break;
                    }
                    if(tmmp->next == NULL)
                    {
                        tmmp->next = (MinMaxPtr) MemNew(sizeof(MinMax));
                        tmmp->next->orgname = tsfbp->name;
                        tmmp->next->min = min;
                        tmmp->next->max = max;
                        tmmp->next->skip = tsfbp->skip;
                        break;
                    }
                }
            }

            if(p == NULL)
                break;
            q = p;
        }
    }
    if(loc != NULL)
        MemFree(loc);

    mmp = mmp->next;
    if(mmp == NULL || mmp->min != 1)
        return(1);

    for(max = mmp->max; mmp != NULL; mmp = mmp->next)
        if(mmp->max > max && mmp->min <= max + 1)
            max = mmp->max;

    if((size_t) max < len)
        return(1);

    tgs = 0;
    count = 0;
    sporg = 0;
    for(tsfbp = sfbp, i = 0; tsfbp != NULL; tsfbp = tsfbp->next, i++)
    {
        if(!tsfbp->full)
            continue;

        if(fta_if_special_org(tsfbp->name))
            sporg++;

        count++;
        if(tsfbp->tg)
            tgs++;
    }

    if(count < 2)
        return(0);
    if(count > 2 || i > count || (tgs != 1 && sporg != 1))
        return(2);
    return(0);
}

/**********************************************************/
static char* CheckWholeSourcesVersusFocused(SourceFeatBlkPtr sfbp)
{
    char* p = NULL;
    bool whole = false;

    for(; sfbp != NULL; sfbp = sfbp->next)
    {
        if(sfbp->full)
            whole = true;
        else if(sfbp->focus)
            p = sfbp->location;
    }

    if(whole)
        return(p);
    return(NULL);
}

/**********************************************************/
static bool CheckSYNTGNDivision(SourceFeatBlkPtr sfbp, char* div)
{
    char* p;
    bool got;
    bool ret;
    Int4    syntgndiv;
    Char    ch;

    syntgndiv = 0;
    if(div != NULL && *div != '\0')
    {
        if(StringCmp(div, "SYN") == 0)
            syntgndiv = 1;
        else if(StringCmp(div, "TGN") == 0)
            syntgndiv = 2;
    }

    for(ret = true, got = false; sfbp != NULL; sfbp = sfbp->next)
    {
        if(!sfbp->tg)
            continue;

        if(syntgndiv == 0)
        {
            p = sfbp->location;
            if(p != NULL && StringLen(p) > 50)
            {
                ch = p[50];
                p[50] = '\0';
            }
            else
                ch = '\0';
            ErrPostEx(SEV_REJECT, ERR_DIVISION_TransgenicNotSYN_TGN,
                      "Source feature located at \"%s\" has a /transgenic qualifier, but this record is not in the SYN or TGN division.",
                      (p == NULL) ? "unknown" : p);
            if(ch != '\0')
                p[50] = ch;
            ret = false;
        }
   
        if(sfbp->full)
            got = true;
    }
    
    if(syntgndiv == 2 && !got)
        ErrPostEx(SEV_ERROR, ERR_DIVISION_TGNnotTransgenic,
                  "This record uses the TGN division code, but there is no full-length /transgenic source feature.");
    return(ret);
}

/**********************************************************/
static Int4 CheckTransgenicSourceFeats(SourceFeatBlkPtr sfbp)
{
    SourceFeatBlkPtr tsfbp;
    char*          taxname;
    bool             same;
    bool             tgfull;

    if(sfbp == NULL)
        return(0);

    Int4 ret = 0;
    bool tgs = false;
    bool focus = false;
    for(tsfbp = sfbp; tsfbp != NULL; tsfbp = tsfbp->next)
    {
        if(tsfbp->tg)
        {
            if(!tsfbp->full)
                ret = 1;                /* /transgenic on not full-length */
            else if(tgs)
                ret = 3;                /* multiple /transgenics */
            if(ret != 0)
                break;
            tgs = true;
        }
        if(tsfbp->focus)
            focus = true;
        if(tgs && focus)
        {
            ret = 2;                    /* /focus and /transgenic */
            break;
        }
    }

    if(ret != 0)
        return(ret);

    same = true;
    tgfull = false;
    taxname = NULL;
    for(tsfbp = sfbp; tsfbp != NULL; tsfbp = tsfbp->next)
    {
        if(tsfbp->skip)
            continue;
        if(taxname == NULL)
            taxname = tsfbp->name;
        else if(same && !fta_strings_same(taxname, tsfbp->name))
            same = false;
        if(tsfbp->tg && tsfbp->full)
            tgfull = true;
        if(tsfbp->focus)
            focus = true;
    }

    if(same == false && tgfull == false && focus == false)
        return(4);

    if(sfbp->next == NULL || !tgs)
        return(0);

    for(tsfbp = sfbp->next; tsfbp != NULL; tsfbp = tsfbp->next)
        if(fta_strings_same(sfbp->name, tsfbp->name) == false ||
           fta_strings_same(sfbp->strain, tsfbp->strain) == false ||
           fta_strings_same(sfbp->isolate, tsfbp->isolate) == false ||
           fta_strings_same(sfbp->organelle, tsfbp->organelle) == false)
            break;

    if(tsfbp == NULL)
        return(5);                      /* all source features have the same
                                           /organism, /strain, /isolate and
                                           /organelle qualifiers */
    return(0);
}

/**********************************************************/
static Int4 CheckFocusInOrgs(SourceFeatBlkPtr sfbp, size_t len, int* status)
{
    SourceFeatBlkPtr tsfbp;
    const char       **b;
    char*          name;
    Char             pat[100];
    Int4             count;
    bool             same;

    count = 0;
    name = NULL;
    same = true;
    for(tsfbp = sfbp; tsfbp != NULL; tsfbp = tsfbp->next)
    {
        if(tsfbp->name == NULL)
            continue;
        if(tsfbp->focus)
            count++;
        if(name == NULL)
            name = tsfbp->name;
        else if(StringICmp(name, tsfbp->name) != 0)
            same = false;
    }
    if(same && count > 0)
        (*status)++;

    name = NULL;
    for(tsfbp = sfbp; tsfbp != NULL; tsfbp = tsfbp->next)
    {
        if(!tsfbp->focus || tsfbp->name == NULL)
            continue;
        if(name == NULL)
            name = tsfbp->name;
        else if(StringICmp(name, tsfbp->name) != 0)
            break;
    }
    if(tsfbp != NULL)
        return(2);

    if(same || count != 0)
        return(0);

    name = NULL;
    sprintf(pat, "1..%ld", len);
    for(tsfbp = sfbp; tsfbp != NULL; tsfbp = tsfbp->next)
    {
        if(tsfbp->name == NULL || tsfbp->location == NULL ||
           tsfbp->skip)
            continue;

        for (b = special_orgs; *b != NULL; b++)
        {
            if (StringICmp(*b, tsfbp->name) == 0 &&
                StringCmp(tsfbp->location, pat) == 0)
                break;
        }
        if(*b != NULL)
            continue;

        if(name == NULL)
/**            name = tsfbp->namstr;*/
            name = tsfbp->name;
/**        else if(StringICmp(name, tsfbp->namstr) != 0)*/
        else if(StringICmp(name, tsfbp->name) != 0)
            break;
    }

    if(tsfbp == NULL)
        return(0);

    for (tsfbp = sfbp; tsfbp != NULL; tsfbp = tsfbp->next)
    {
        if (tsfbp->full && tsfbp->tg && !tsfbp->skip)
            break;
    }

    if(tsfbp != NULL)
        return(0);
    return(3);
}

/**********************************************************/
static bool IfSpecialFeat(MinMaxPtr mmp, size_t len)
{
    if((mmp->min == 1 && (size_t) mmp->max == len) || mmp->skip)
        return true;
    return false;
}

/**********************************************************/
static char* CheckSourceOverlap(MinMaxPtr mmp, size_t len)
{
    MinMaxPtr tmmp;
    char*   res;

    for(; mmp != NULL; mmp = mmp->next)
    {
        if(IfSpecialFeat(mmp, len))
            continue;
        for(tmmp = mmp->next; tmmp != NULL; tmmp = tmmp->next)
        {
            if(IfSpecialFeat(tmmp, len))
                continue;
            if(StringICmp(mmp->orgname, tmmp->orgname) == 0)
                continue;
            if(tmmp->min <= mmp->max && tmmp->max >= mmp->min)
                break;
        }
        if(tmmp != NULL)
            break;
    }
    if(mmp == NULL)
        return(NULL);

    res = (char*) MemNew(1024);
    sprintf(res, "\"%s\" at %d..%d vs \"%s\" at %d..%d", mmp->orgname,
            mmp->min, mmp->max, tmmp->orgname, tmmp->min, tmmp->max);
    return(res);
}

/**********************************************************/
static char* CheckForUnusualFullLengthOrgs(SourceFeatBlkPtr sfbp)
{
    SourceFeatBlkPtr tsfbp;
    const char       **b;

    if(sfbp == NULL || sfbp->next == NULL)
        return(NULL);

    for(tsfbp = sfbp->next; tsfbp != NULL; tsfbp = tsfbp->next)
        if(StringICmp(sfbp->name, tsfbp->name) != 0)
            break;

    if(tsfbp == NULL)
        return(NULL);

    for(tsfbp = sfbp; tsfbp != NULL; tsfbp = tsfbp->next)
        if(tsfbp->full && tsfbp->tg)
            break;

    if(tsfbp != NULL)
        return(NULL);

    for(; sfbp != NULL; sfbp = sfbp->next)
    {
        if(!sfbp->full || sfbp->tg)
            continue;

        for(b = special_orgs; *b != NULL; b++)
            if(StringICmp(*b, sfbp->name) == 0)
                break;

        if(*b != NULL)
            continue;

        if(StringIStr(sfbp->name, "vector") == NULL)
            break;
    }
    if(sfbp == NULL)
        return(NULL);
    return(sfbp->name);
}

/**********************************************************/
static void CreateRawBioSources(ParserPtr pp, SourceFeatBlkPtr sfbp,
                                Int4 use_what)
{
    SourceFeatBlkPtr tsfbp;
    char*          namstr;
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

    for(; sfbp != NULL; sfbp = sfbp->next)
    {
        if (sfbp->bio_src.NotEmpty())
            continue;

        namstr = StringSave(sfbp->namstr);
        CRef<objects::COrg_ref> org_ref(new objects::COrg_ref);
        org_ref->SetTaxname(sfbp->name);

        if (sfbp->orgname.NotEmpty())
        {
            org_ref->SetOrgname(*sfbp->orgname);
        }

        CRef<objects::COrg_ref> t_org_ref(new objects::COrg_ref);
        t_org_ref->Assign(*org_ref);
        fta_fix_orgref(pp, *org_ref, &pp->entrylist[pp->curindx]->drop, sfbp->genomename);

        if (t_org_ref->Equals(*org_ref))
            sfbp->lookup = false;
        else
        {
            sfbp->lookup = true;
            MemFree(sfbp->name);
            sfbp->name = StringSave(org_ref->GetTaxname().c_str());

            sfbp->orgname.Reset();

            cultivar = NULL;
            isolate = NULL;
            serotype = NULL;
            serovar = NULL;
            ecotype = NULL;
            specimen_voucher = NULL;
            strain = NULL;
            sub_species = NULL;
            sub_strain = NULL;
            variety = NULL;
            if (org_ref->IsSetOrgname() && org_ref->IsSetOrgMod())
            {
                ITERATE(objects::COrgName::TMod, mod, org_ref->GetOrgname().GetMod())
                {
                    switch ((*mod)->GetSubtype())
                    {
                    case 10:
                        cultivar = (*mod)->GetSubname().c_str();
                        break;
                    case 17:
                        isolate = (*mod)->GetSubname().c_str();
                        break;
                    case 7:
                        serotype = (*mod)->GetSubname().c_str();
                        break;
                    case 9:
                        serovar = (*mod)->GetSubname().c_str();
                        break;
                    case 27:
                        ecotype = (*mod)->GetSubname().c_str();
                        break;
                    case 23:
                        specimen_voucher = (*mod)->GetSubname().c_str();
                        break;
                    case 2:
                        strain = (*mod)->GetSubname().c_str();
                        break;
                    case 22:
                        sub_species = (*mod)->GetSubname().c_str();
                        break;
                    case 3:
                        sub_strain = (*mod)->GetSubname().c_str();
                        break;
                    case 6:
                        variety = (*mod)->GetSubname().c_str();
                        break;
                    }
                }
            }
            CollectSubNames(sfbp, use_what, sfbp->name, cultivar, isolate,
                            serotype, serovar, specimen_voucher, strain,
                            sub_species, sub_strain, variety, ecotype);
        }

        sfbp->bio_src.Reset(new objects::CBioSource);
        sfbp->bio_src->SetOrg(*org_ref);

        for(tsfbp = sfbp->next; tsfbp != NULL; tsfbp = tsfbp->next)
        {
            if(tsfbp->bio_src.NotEmpty() || StringICmp(namstr, tsfbp->namstr) != 0)
                continue;
            
            tsfbp->lookup = sfbp->lookup;

            tsfbp->bio_src.Reset(new objects::CBioSource);
            tsfbp->bio_src->Assign(*sfbp->bio_src);

            if(!sfbp->lookup)
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

    if(what == where)
        return(where);

    prev = where;
    for(tsfbp = where->next; tsfbp != NULL; tsfbp = tsfbp->next)
    {
        if(tsfbp == what)
            break;
        prev = tsfbp;
    }
    if(tsfbp == NULL)
        return(where);

    prev->next = what->next;
    what->next = where;
    return(what);
}

/**********************************************************/
static SourceFeatBlkPtr SourceFeatRemoveDups(SourceFeatBlkPtr sfbp)
{
    SourceFeatBlkPtr tsfbp;
    SourceFeatBlkPtr prev;
    SourceFeatBlkPtr next;

    for(prev = sfbp, tsfbp = sfbp->next; tsfbp != NULL; tsfbp = next)
    {
        next = tsfbp->next;
        if(!tsfbp->useit)
        {
            prev = tsfbp;
            continue;
        }

        bool different = false;
        ITERATE(TQualVector, cur, tsfbp->quals)
        {
            const std::string& cur_qual = (*cur)->GetQual();
            if (cur_qual == "focus")
                continue;

            bool found = false;
            ITERATE(TQualVector, next, sfbp->quals)
            {
                const std::string& next_qual = (*next)->GetQual();

                if (next_qual == "focus" || next_qual != cur_qual)
                    continue;

                if (!(*cur)->IsSetVal() && !(*next)->IsSetVal())
                {
                    found = true;
                    break;
                }

                if ((*cur)->IsSetVal() && (*next)->IsSetVal() &&
                    (*cur)->GetVal() == (*next)->GetVal())
                {
                    found = true;
                    break;
                }
            }

            if (!found)              /* Different, leave as is */
            {
                different = true;
                break;
            }
        }

        if (different)                /* Different, leave as is */
        {
            prev = tsfbp;
            continue;
        }
        prev->next = tsfbp->next;
        tsfbp->next = NULL;
        SourceFeatBlkFree(tsfbp);
    }
    return(sfbp);
}

/**********************************************************/
static SourceFeatBlkPtr SourceFeatDerive(SourceFeatBlkPtr sfbp,
                                         SourceFeatBlkPtr res)
{
    SourceFeatBlkPtr tsfbp;

    if(res == NULL)
        return(sfbp);

    tsfbp = SourceFeatBlkNew();
    tsfbp->name = (res->name == NULL) ? NULL : StringSave(res->name);
    tsfbp->namstr = (res->namstr == NULL) ? NULL : StringSave(res->namstr);
    tsfbp->location = (res->location == NULL) ? NULL : StringSave(res->location);
    tsfbp->full = res->full;
    tsfbp->focus = res->focus;
    tsfbp->lookup = res->lookup;
    tsfbp->genome = res->genome;
    tsfbp->next = NULL;

    tsfbp->bio_src.Reset(new objects::CBioSource);
    tsfbp->bio_src->Assign(*res->bio_src);

    tsfbp->orgname.Reset(new objects::COrgName);
    if (res->orgname.NotEmpty())
        tsfbp->orgname->Assign(*res->orgname);

    tsfbp->quals = res->quals;
    tsfbp->next = sfbp;
    sfbp = tsfbp;

    for (TQualVector::iterator cur = sfbp->quals.begin(); cur != sfbp->quals.end(); )
    {
        const std::string& cur_qual = (*cur)->GetQual();
        if (cur_qual == "focus")
        {
            ++cur;
            continue;
        }

        for(tsfbp = sfbp->next; tsfbp != NULL; tsfbp = tsfbp->next)
        {
            if(tsfbp == res || !tsfbp->useit)
                continue;

            bool found = false;
            ITERATE(TQualVector, next, tsfbp->quals)
            {
                const std::string& next_qual = (*next)->GetQual();

                if (next_qual == "focus" || next_qual != cur_qual)
                    continue;

                if (!(*cur)->IsSetVal() && !(*next)->IsSetVal())
                {
                    found = true;
                    break;
                }

                if ((*cur)->IsSetVal() && (*next)->IsSetVal() &&
                    (*cur)->GetVal() == (*next)->GetVal())
                {
                    found = true;
                    break;
                }
            }

            if (!found)            /* Not found */
                break;
        }

        if (tsfbp == NULL)               /* Got the match */
        {
            ++cur;
            continue;
        }

        cur = sfbp->quals.erase(cur);
    }

    return(SourceFeatRemoveDups(sfbp));
}

/**********************************************************/
static SourceFeatBlkPtr PickTheDescrSource(SourceFeatBlkPtr sfbp)
{
    SourceFeatBlkPtr res;
    SourceFeatBlkPtr tsfbp;

    if(sfbp->next == NULL)
    {
        if(!sfbp->full)
        {
            ErrPostEx(SEV_WARNING, ERR_SOURCE_SingleSourceTooShort,
                      "Source feature does not span the entire length of the sequence.");
        }
        return(sfbp);
    }

    Int4 count_skip = 0;
    Int4 count_noskip = 0;
    bool same = true;
    for(tsfbp = sfbp; tsfbp != NULL; tsfbp = tsfbp->next)
    {
        if(StringICmp(tsfbp->name, sfbp->name) != 0)
        {
            same = false;
            break;
        }

        if(!tsfbp->skip)
        {
            res = tsfbp;
            count_noskip++;
        }
        else
            count_skip++;
    }
        
    if(same)
    {
        if(count_noskip == 1)
        {
            sfbp = SourceFeatMoveOneUp(sfbp, res);
            return(SourceFeatRemoveDups(sfbp));
        }
        for(res = NULL, tsfbp = sfbp; tsfbp != NULL; tsfbp = tsfbp->next)
        {
            if(count_noskip != 0 && tsfbp->skip)
                continue;
            tsfbp->useit = true;
            if(res == NULL)
                res = tsfbp;
        }
        return(SourceFeatDerive(sfbp, res));
    }

    for (tsfbp = sfbp; tsfbp != NULL; tsfbp = tsfbp->next)
    {
        if (tsfbp->tg)
            break;
    }
    if(tsfbp != NULL)
        return(SourceFeatMoveOneUp(sfbp, tsfbp));

    for(res = NULL, tsfbp = sfbp; tsfbp != NULL; tsfbp = tsfbp->next)
    {
        if(!tsfbp->focus)
            continue;
        res = tsfbp;
        if(!tsfbp->skip)
            break;
    }

    if(res != NULL)
    {
        count_skip = 0;
        count_noskip = 0;
        for(tsfbp = sfbp; tsfbp != NULL; tsfbp = tsfbp->next)
        {
            if(StringICmp(res->name, tsfbp->name) != 0)
                continue;
            tsfbp->useit = true;
            if(tsfbp->skip)
                count_skip++;
            else
                count_noskip++;
        }
        if(count_noskip > 0)
        {
            for(tsfbp = sfbp; tsfbp != NULL; tsfbp = tsfbp->next)
            {
                if(StringICmp(res->name, tsfbp->name) != 0)
                    continue;
                if(res != tsfbp && tsfbp->skip)
                    tsfbp->useit = false;
            }
        }
        return(SourceFeatDerive(sfbp, res));
    }

    for(tsfbp = sfbp; tsfbp != NULL; tsfbp = tsfbp->next)
    {
        if(!tsfbp->full)
            continue;
        res = tsfbp;
        break;
    }
    if(res != NULL)
    {
        sfbp = SourceFeatMoveOneUp(sfbp, res);
        return(SourceFeatRemoveDups(sfbp));
    }

    SourceFeatBlkSetFree(sfbp);
    ErrPostEx(SEV_ERROR, ERR_SOURCE_MissingSourceFeatureForDescr,
              "Could not select the right source feature among different organisms to create descriptor: no /focus and 1..N one. Entry dropped.");
    return(NULL);
}

/**********************************************************/
static void AddOrgMod(objects::COrg_ref& org_ref, const Char* val, Uint1 type)
{
    objects::COrgName& orgname = org_ref.SetOrgname();

    CRef<objects::COrgMod> mod(new objects::COrgMod);
    mod->SetSubtype(type);
    mod->SetSubname((val == NULL) ? "" : val);

    orgname.SetMod().push_back(mod);
}

/**********************************************************/
static void FTASubSourceAdd(objects::CBioSource& bio, const Char* val, Uint1 type)
{
    if (type != 12)                      /* dev-stage */
    {
        bool found = false;
        ITERATE(objects::CBioSource::TSubtype, subtype, bio.GetSubtype())
        {
            if ((*subtype)->GetSubtype() == type)
            {
                found = true;
                break;
            }
        }

        if (found)
            return;
    }

    CRef<objects::CSubSource> sub(new objects::CSubSource);
    sub->SetSubtype(type);
    sub->SetName((val == NULL) ? "" : val);
    bio.SetSubtype().push_back(sub);
}

/**********************************************************/
static void CheckQualsInSourceFeat(objects::CBioSource& bio, TQualVector& quals,
                                   Uint1 taxserver)
{
    const Char **b;

    char*    p;

    if (!bio.CanGetOrg())
        return;

    std::vector<std::string> modnames;

    if (bio.GetOrg().CanGetOrgname() && bio.GetOrg().GetOrgname().CanGetMod())
    {
        ITERATE(objects::COrgName::TMod, mod, bio.GetOrg().GetOrgname().GetMod())
        {
            for (size_t i = 0; SourceOrgMods[i].name != NULL; ++i)
            {
                if(SourceOrgMods[i].num != (*mod)->GetSubtype())
                    continue;

                modnames.push_back(SourceOrgMods[i].name);
                break;
            }
        }
    }

    ITERATE(TQualVector, cur, quals)
    {
        if (!(*cur)->IsSetQual() || (*cur)->GetQual() == "organism")
            continue;

        const std::string& cur_qual = (*cur)->GetQual();
        const Char* val_ptr = (*cur)->IsSetVal() ? (*cur)->GetVal().c_str() : NULL;

        if (cur_qual == "note")
        {
            FTASubSourceAdd(bio, val_ptr, 255);
            continue;
        }

        for(b = SourceBadQuals; *b != NULL; b++)
        {
            if (cur_qual != *b)
                continue;

            if (val_ptr == NULL || val_ptr[0] == '\0')
                p = StringSave("???");
            else
                p = StringSave(val_ptr);
            if(StringLen(p) > 50)
                p[50] = '\0';
            ErrPostEx(SEV_WARNING, ERR_SOURCE_UnwantedQualifiers,
                      "Unwanted qualifier on source feature: %s=%s",
                      cur_qual.c_str(), p);
            MemFree(p);
        }

        b = SourceSubSources;
        for (size_t i = 1; *b != NULL; i++, b++)
        {
            if (**b != '\0' && cur_qual == *b)
            {
                FTASubSourceAdd(bio, val_ptr, (Uint1)i);
                break;
            }
        }

        if (cur_qual == "organism" ||
           (taxserver != 0 && cur_qual == "type_material"))
            continue;

        if (find(modnames.begin(), modnames.end(), cur_qual) != modnames.end())
            continue;

        for (size_t i = 0; SourceOrgMods[i].name != NULL; i++)
        {
            if (cur_qual == SourceOrgMods[i].name)
            {
                AddOrgMod(bio.SetOrg(), val_ptr, SourceOrgMods[i].num);
                break;
            }
        }
    }
}

/**********************************************************/
static CRef<objects::CDbtag> GetSourceDbtag(CRef<objects::CGb_qual>& qual, Parser::ESource source)
{
    const char **b;
    const char *q;
    char*    line;
    char*    p;

    CRef<objects::CDbtag> tag;

    if (qual->GetQual() != "db_xref")
        return tag;

    std::vector<Char> val_buf(qual->GetVal().begin(), qual->GetVal().end());
    val_buf.push_back(0);

    p = StringChr(&val_buf[0], ':');
    if(p == NULL || p[1] == '\0')
        return tag;

    *p = '\0';
    if (StringICmp(&val_buf[0], "taxon") == 0)
    {
        *p = ':';
        return tag;
    }

    if(source == Parser::ESource::NCBI)
        q = "NCBI";
    else if(source == Parser::ESource::EMBL)
        q = "EMBL";
    else if(source == Parser::ESource::DDBJ)
        q = "DDBJ";
    else if(source == Parser::ESource::SPROT)
        q = "SwissProt";
    else if(source == Parser::ESource::PIR)
        q = "PIR";
    else if(source == Parser::ESource::LANL)
        q = "LANL";
    else if(source == Parser::ESource::Refseq)
        q = "RefSeq";
    else
        q = "Unknown";

    if(source != Parser::ESource::NCBI && source != Parser::ESource::DDBJ &&
       source != Parser::ESource::EMBL && source != Parser::ESource::LANL &&
       source != Parser::ESource::Refseq)
    {
        *p = ':';
        ErrPostEx(SEV_ERROR, ERR_SOURCE_InvalidDbXref,
                  "Cannot process source feature's \"/db_xref=%s\" for source \"%s\".",
                  &val_buf[0], q);
        return tag;
    }

    for (b = ObsoleteSourceDbxrefTag; *b != NULL; b++)
    {
        if (StringICmp(*b, &val_buf[0]) == 0)
            break;
    }

    if(*b != NULL)
    {
        ErrPostEx(SEV_WARNING, ERR_SOURCE_ObsoleteDbXref,
                  "/db_xref type \"%s\" is obsolete.", &val_buf[0]);
        if (StringICmp(&val_buf[0], "IFO") == 0)
        {
            line = (char*) MemNew(25 + StringLen(p + 1));
            StringCpy(line, "NBRC:");
            StringCat(line, p + 1);
            qual->SetVal(line);
            MemFree(line);

            val_buf.assign(line, line + StringLen(line));
            val_buf.push_back(0);

            p = &val_buf[0] + 4;
            *p = '\0';
        }
    }

    for (b = DENLRSourceDbxrefTag; *b != NULL; b++)
    {
        if (StringICmp(*b, &val_buf[0]) == 0)
            break;
    }

    if(*b == NULL && (source == Parser::ESource::DDBJ || source == Parser::ESource::EMBL))
    {
        for(b = DESourceDbxrefTag; *b != NULL; b++)
            if (StringICmp(*b, &val_buf[0]) == 0)
                break;
    }
    if(*b == NULL && source == Parser::ESource::EMBL)
    {
        for(b = ESourceDbxrefTag; *b != NULL; b++)
            if (StringICmp(*b, &val_buf[0]) == 0)
                break;
    }
    if(*b == NULL && (source == Parser::ESource::NCBI || source == Parser::ESource::LANL ||
                      source == Parser::ESource::Refseq))
    {
        for (b = NLRSourceDbxrefTag; *b != NULL; b++)
        {
            if (StringICmp(*b, &val_buf[0]) == 0)
                break;
        }
    }
    
    if(*b == NULL)
    {
        *p = ':';
        ErrPostEx(SEV_ERROR, ERR_SOURCE_InvalidDbXref,
                  "Invalid database name in source feature's \"/db_xref=%s\" for source \"%s\".",
                  &val_buf[0], q);
        return tag;
    }

    tag.Reset(new objects::CDbtag);
    tag->SetDb(&val_buf[0]);

    *p++ = ':';
    for(q = p; *p >= '0' && *p <= '9';)
         p++;

    if(*p == '\0' && *q != '0')
        tag->SetTag().SetId(atoi(q));
    else
        tag->SetTag().SetStr(q);

    return tag;
}

/**********************************************************/
static bool UpdateRawBioSource(SourceFeatBlkPtr sfbp, Parser::ESource source, IndexblkPtr ibp, Uint1 taxserver)
{
    char*      div;
    char*      tco;
    char*      p;
    char*      q;

    Int4         newgen;
    Int4         oldgen;
    Int2         i;

    bool is_syn = false;
    bool is_pat = false;

    div = ibp->division;
    if(div != NULL)
    {
        if(StringCmp(div, "SYN") == 0)
            is_syn = true;
        else if(StringCmp(div, "PAT") == 0)
            is_pat = true;
    }
    for(; sfbp != NULL; sfbp = sfbp->next)
    {
        if (sfbp->bio_src.Empty())
            continue;

        objects::CBioSource& bio = *sfbp->bio_src;

        if(!sfbp->lookup)
        {
            if(is_syn && !sfbp->tg)
                bio.SetOrigin(4);        /* artificial */
        }
        else
        {
            if (bio.CanGetOrg() && bio.GetOrg().CanGetOrgname() &&
                bio.GetOrg().GetOrgname().CanGetDiv() &&
                bio.GetOrg().GetOrgname().GetDiv() == "SYN")
            {
                bio.SetOrigin(4);        /* artificial */
                if (is_syn == false && is_pat == false)
                {
                    const Char* taxname = NULL;
                    if (bio.GetOrg().CanGetTaxname() &&
                        !bio.GetOrg().GetTaxname().empty())
                        taxname = bio.GetOrg().GetTaxname().c_str();
                    ErrPostEx(SEV_ERROR, ERR_ORGANISM_SynOrgNameNotSYNdivision,
                              "The NCBI Taxonomy DB believes that organism name \"%s\" is reserved for synthetic sequences, but this record is not in the SYN division.",
                              (taxname == NULL) ? "not_specified" : taxname);
                }
            }
        }

        newgen = -1;
        oldgen = -1;

        bool dropped = false;
        NON_CONST_ITERATE(TQualVector, cur, sfbp->quals)
        {
            if (!(*cur)->IsSetQual() || (*cur)->GetQual().empty())
                continue;

            const std::string& cur_qual = (*cur)->GetQual();
            if (cur_qual == "db_xref")
            {
                CRef<objects::CDbtag> dbtag = GetSourceDbtag(*cur, source);
                if (dbtag.Empty())
                    continue;

                bio.SetOrg().SetDb().push_back(dbtag);
                continue;
            }

            const Char* val_ptr = (*cur)->IsSetVal() ? (*cur)->GetVal().c_str() : NULL;
            if (cur_qual == "organelle")
            {
                if (val_ptr == NULL || val_ptr[0] == '\0')
                    continue;

                p = StringChr(val_ptr, ':');
                if (p != NULL)
                {
                    if (StringChr(p + 1, ':') != NULL)
                    {
                        ErrPostEx(SEV_ERROR, ERR_SOURCE_OrganelleQualMultToks,
                                  "More than 2 tokens found in /organelle qualifier: \"%s\". Entry dropped.",
                                  val_ptr);
                        dropped = true;
                        break;
                    }

                    std::string val_str(val_ptr, static_cast<const Char*>(p));
                    i = StringMatchIcase(OrganelleFirstToken, val_str.c_str());
                    if(i < 0)
                    {
                        ErrPostEx(SEV_ERROR, ERR_SOURCE_OrganelleIllegalClass,
                                  "Illegal class in /organelle qualifier: \"%s\". Entry dropped.",
                                  val_ptr);
                        dropped = true;
                        break;
                    }
                    if(i == 4)
                        ibp->got_plastid = true;
                    if(newgen < 0)
                        newgen = StringMatchIcase(GenomicSourceFeatQual,
                                                  p + 1);
                }
                else
                {
                    i = StringMatchIcase(OrganelleFirstToken, val_ptr);
                    if(i < 0)
                    {
                        ErrPostEx(SEV_ERROR, ERR_SOURCE_OrganelleIllegalClass,
                                  "Illegal class in /organelle qualifier: \"%s\". Entry dropped.",
                                  val_ptr);
                        dropped = true;
                        break;
                    }
                    if(i == 4)
                        ibp->got_plastid = true;
                    if(newgen < 0)
                        newgen = StringMatchIcase(GenomicSourceFeatQual,
                                                  val_ptr);
                }
                continue;
            }

            if(oldgen < 0)
                oldgen = StringMatchIcase(GenomicSourceFeatQual, cur_qual.c_str());

            if (cur_qual != "country" ||
                val_ptr == NULL || val_ptr[0] == '\0')
                continue;

            tco = StringSave(val_ptr);
            p = StringChr(tco, ':');
            if(p != NULL)
                *p = '\0';
            for(p = tco; *p == ' ' || *p == '\t';)
                p++;
            if(*p == '\0')
            {
                ErrPostEx(SEV_ERROR, ERR_SOURCE_InvalidCountry,
                          "Empty country name in /country qualifier : \"%s\".",
                          val_ptr);
            }
            else
            {
                for(q = p + 1; *q != '\0';)
                    q++;
                for(q--; *q == ' ' || *q == '\t';)
                    q--;
                *++q = '\0';

                bool valid_country = objects::CCountries::IsValid(p);
                if (!valid_country)
                {
                    valid_country = objects::CCountries::WasValid(p);

                    if (!valid_country)
                        ErrPostEx(SEV_ERROR, ERR_SOURCE_InvalidCountry,
                        "Country \"%s\" from /country qualifier \"%s\" is not a valid country name.",
                        tco, val_ptr);
                    else
                        ErrPostEx(SEV_WARNING, ERR_SOURCE_FormerCountry,
                        "Country \"%s\" from /country qualifier \"%s\" is a former country name which is no longer valid.",
                        tco, val_ptr);
                }
            }

            MemFree(tco);
            FTASubSourceAdd(bio, val_ptr, 23);
        }

        if (dropped)
            break;

        if (newgen > -1)
            bio.SetGenome(newgen);
        else if (oldgen > -1)
            bio.SetGenome(oldgen);
        else if (sfbp->genome != 0)
            bio.SetGenome(sfbp->genome);

        CheckQualsInSourceFeat(bio, sfbp->quals, taxserver);
        fta_sort_biosource(bio);
    }

    if(sfbp != NULL)
        return false;

    return true;
}


/**********************************************************/
static bool is_a_space_char(Char c)
{
    return c == ' ' || c == '\t';
}

/**********************************************************/
static void CompareDescrFeatSources(SourceFeatBlkPtr sfbp, const objects::CBioseq& bioseq)
{
    SourceFeatBlkPtr tsfbp;

    if(sfbp == NULL || !bioseq.IsSetDescr())
        return;

    ITERATE(objects::CSeq_descr::Tdata, descr, bioseq.GetDescr().Get())
    {
        if (!(*descr)->IsSource())
            continue;

        const objects::CBioSource& bio_src = (*descr)->GetSource();

        if (!bio_src.IsSetOrg() || !bio_src.GetOrg().IsSetTaxname() ||
            bio_src.GetOrg().GetTaxname().empty())
            continue;

        const std::string& taxname = bio_src.GetOrg().GetTaxname();
        std::string orgdescr;
        std::remove_copy_if(taxname.begin(), taxname.end(), std::back_inserter(orgdescr), is_a_space_char);

        std::string commdescr;
        if (bio_src.GetOrg().IsSetCommon())
        {
            const std::string& common = bio_src.GetOrg().GetCommon();
            std::remove_copy_if(common.begin(), common.end(), std::back_inserter(commdescr), is_a_space_char);
        }

        for (tsfbp = sfbp; tsfbp != NULL; tsfbp = tsfbp->next)
        {
            if (tsfbp->name == NULL || tsfbp->name[0] == '\0')
                continue;

            size_t name_len = strlen(tsfbp->name);
            std::string orgfeat;
            std::remove_copy_if(tsfbp->name, tsfbp->name + name_len, std::back_inserter(orgfeat), is_a_space_char);

            if(StringICmp(orgdescr.c_str(), "unknown") == 0)
            {
                if(StringICmp(orgdescr.c_str(), orgfeat.c_str()) == 0 ||
                   (!commdescr.empty() && StringICmp(commdescr.c_str(), orgfeat.c_str()) == 0))
                {
                    break;
                }
            }
            else
            {
                if (orgdescr == orgfeat || commdescr == orgfeat)
                {
                    break;
                }
            }
        }

        if(tsfbp == NULL)
        {
            ErrPostEx(SEV_ERROR, ERR_ORGANISM_NoSourceFeatMatch,
                      "Organism name \"%s\" from OS/ORGANISM line does not exist in this record's source features.",
                      taxname.c_str());
        }
    }
}

/**********************************************************/
static bool CheckSourceLineage(SourceFeatBlkPtr sfbp, Parser::ESource source, bool is_pat)
{
    const Char* p;
    ErrSev  sev;

    for(; sfbp != NULL; sfbp = sfbp->next)
    {
        if(!sfbp->lookup || sfbp->bio_src.Empty() || !sfbp->bio_src->IsSetOrg())
            continue;

        p = NULL;
        if (sfbp->bio_src->GetOrg().IsSetOrgname() && 
            sfbp->bio_src->GetOrg().GetOrgname().IsSetLineage())
            p = sfbp->bio_src->GetOrg().GetOrgname().GetLineage().c_str();

        if (p == NULL || *p == '\0')
        {
            if ((source == Parser::ESource::DDBJ || source == Parser::ESource::EMBL) && is_pat)
                sev = SEV_WARNING;
            else
                sev = SEV_REJECT;
            ErrPostEx(sev, ERR_SERVER_NoLineageFromTaxon,
                      "Taxonomy lookup for organism name \"%s\" yielded an Org-ref that has no lineage.",
                      sfbp->name);
            if(sev == SEV_REJECT)
                break;
        }
    }
    if(sfbp == NULL)
        return true;
    return false;
}

/**********************************************************/
static void PropogateSuppliedLineage(objects::CBioseq& bioseq,
                                     SourceFeatBlkPtr sfbp, Uint1 taxserver)
{
    SourceFeatBlkPtr tsfbp;

    const Char       *p;

    if (!bioseq.IsSetDescr() || sfbp == NULL)
        return;

    for(; sfbp != NULL; sfbp = sfbp->next)
    {
        if(sfbp->lookup || sfbp->bio_src.Empty() ||
           !sfbp->bio_src->IsSetOrg() || !sfbp->bio_src->GetOrg().IsSetTaxname() ||
           sfbp->name == NULL || *sfbp->name == '\0' ||
           sfbp->bio_src->GetOrg().GetTaxname().empty())
            continue;

        objects::COrgName& orgname = sfbp->bio_src->SetOrg().SetOrgname();

        if (orgname.IsSetLineage())
        {
            if (!orgname.GetLineage().empty())
                continue;

            orgname.ResetLineage();
        }

        const std::string& taxname = sfbp->bio_src->GetOrg().GetTaxname();
        std::string lineage;

        bool found = false;
        ITERATE(objects::CSeq_descr::Tdata, descr, bioseq.GetDescr().Get())
        {
            if (!(*descr)->IsSource())
                continue;

            const objects::CBioSource& bio_src = (*descr)->GetSource();

            if (!bio_src.IsSetOrg() || !bio_src.GetOrg().IsSetOrgname() ||
                !bio_src.GetOrg().IsSetTaxname() || bio_src.GetOrg().GetTaxname().empty() ||
                !bio_src.GetOrg().GetOrgname().IsSetLineage())
                continue;

            lineage = bio_src.GetOrg().GetOrgname().GetLineage();
            const std::string& cur_taxname = bio_src.GetOrg().GetTaxname();

            if (StringICmp(cur_taxname.c_str(), taxname.c_str()) == 0)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            ErrPostEx((taxserver == 0) ? SEV_INFO : SEV_WARNING,
                      ERR_ORGANISM_UnclassifiedLineage,
                      "Taxonomy lookup for organism name \"%s\" failed, and no matching organism exists in OS/ORGANISM lines, so lineage has been set to \"Unclassified\".",
                      taxname.c_str());
            p = "Unclassified";
        }
        else
        {
            if (lineage.empty())
            {
                ErrPostEx((taxserver == 0) ? SEV_INFO : SEV_WARNING,
                          ERR_ORGANISM_UnclassifiedLineage,
                          "Taxonomy lookup for organism name \"%s\" failed, and the matching organism from OS/ORGANISM lines has no lineage, so lineage has been set to \"Unclassified\".",
                          taxname.c_str());
                p = "Unclassified";
            }
            else
                p = lineage.c_str();
        }

        orgname.SetLineage(p);
        for(tsfbp = sfbp->next; tsfbp != NULL; tsfbp = tsfbp->next)
        {
            if (tsfbp->lookup || tsfbp->bio_src.Empty() ||
                !tsfbp->bio_src->IsSetOrg() || !tsfbp->bio_src->GetOrg().IsSetTaxname() ||
                tsfbp->name == NULL || *tsfbp->name == '\0' ||
                tsfbp->bio_src->GetOrg().GetTaxname().empty() ||
                StringICmp(sfbp->name, tsfbp->name) != 0)
                
                continue;

            objects::COrgName& torgname = tsfbp->bio_src->SetOrg().SetOrgname();

            if (torgname.IsSetLineage())
            {
                if (!torgname.GetLineage().empty())
                    continue;
            }
            torgname.SetLineage(p);
        }
    }
}

/**********************************************************/
static bool CheckMoltypeConsistency(SourceFeatBlkPtr sfbp, char** moltype)
{
    SourceFeatBlkPtr tsfbp;
    char*          name;
    char*          p;
    bool             ret;
    Char             ch;

    if(sfbp == NULL)
        return true;

    for(tsfbp = sfbp; tsfbp != NULL; tsfbp = tsfbp->next)
        if(tsfbp->moltype != NULL)
            break;

    if(tsfbp == NULL)
        return true;

    name = tsfbp->moltype;
    for(ret = true, tsfbp = sfbp; tsfbp != NULL; tsfbp = tsfbp->next)
    {
        if(tsfbp->moltype == NULL)
        {
            ch = '\0';
            p = tsfbp->location;
            if(p != NULL && StringLen(p) > 50)
            {
                ch = p[50];
                p[50] = '\0';
            }
            ErrPostEx(SEV_ERROR, ERR_SOURCE_MissingMolType,
                      "Source feature at \"%s\" lacks a /mol_type qualifier.",
                      (p == NULL) ? "<empty>" : p);
            if(ch != '\0')
                p[50] = ch;
        }
        else if(ret && StringCmp(name, tsfbp->moltype) != 0)
            ret = false;
    }

    if(ret)
        *moltype = StringSave(name);

    return(ret);
}

/**********************************************************/
static bool CheckForENV(SourceFeatBlkPtr sfbp, IndexblkPtr ibp, Parser::ESource source)
{
    const char **b;

    char*    location;
    Int4       sources;
    Int4       envs;
    Char       ch;

    if(sfbp == NULL || ibp == NULL)
        return true;

    bool skip = false;
    location = NULL;
    ibp->env_sample_qual = false;
    for(envs = 0, sources = 0; sfbp != NULL; sfbp = sfbp->next, sources++)
    {
        bool env_found = false;
        ITERATE(TQualVector, cur, sfbp->quals)
        {
            if ((*cur)->IsSetQual() && (*cur)->GetQual() == "environmental_sample")
            {
                env_found = true;
                break;
            }
        }
        if (env_found)
            envs++;
        else
            location = sfbp->location;

        if(!sfbp->full || sfbp->name == NULL || sfbp->name[0] == '\0')
            continue;

        for (b = special_orgs; *b != NULL; b++)
        {
            if (StringICmp(*b, sfbp->name) == 0)
                break;
        }
        if(*b != NULL)
            skip = true;
    }

    if(envs > 0)
    {
        ibp->env_sample_qual = true;
        if(!skip && envs != sources)
        {
            if(location != NULL && StringLen(location) > 50)
            {
                ch = location[50];
                location[50] = '\0';
            }
            else
                ch = '\0';
            ErrPostEx(SEV_REJECT, ERR_SOURCE_InconsistentEnvSampQual,
                      "Inconsistent /environmental_sample qualifier usage. Source feature at location \"%s\" lacks the qualifier.",
                      (location == NULL) ? "unknown" : location);
            if(ch != '\0')
                location[50] = ch;
            return false;
        }
    }
    else if(StringICmp(ibp->division, "ENV") == 0)
    {
        if(source == Parser::ESource::EMBL)
            ErrPostEx(SEV_ERROR, ERR_SOURCE_MissingEnvSampQual,
                      "This ENV division record has source features that lack the /environmental_sample qualifier. It will not be placed in the ENV division until the qualifier is added.");
        else
        {
            ErrPostEx(SEV_REJECT, ERR_SOURCE_MissingEnvSampQual,
                      "This ENV division record has source features that lack the /environmental_sample qualifier.");
            return false;
        }
    }
    return true;
}

/**********************************************************/
static char* CheckPcrPrimersTag(char* str)
{
    if(StringNCmp(str, "fwd_name", 8) == 0 ||
       StringNCmp(str, "rev_name", 8) == 0)
        str += 8;
    else if(StringNCmp(str, "fwd_seq", 7) == 0 ||
            StringNCmp(str, "rev_seq", 7) == 0)
        str += 7;
    else
        return(NULL);

    if(*str == ' ')
        str++;
    if(*str == ':')
        return(str + 1);
    return(NULL);
}

/**********************************************************/
static void PopulatePcrPrimers(objects::CBioSource& bio, PcrPrimersPtr ppp, Int4 count)
{
    PcrPrimersPtr tppp;

    char*       str_fs;
    char*       str_rs;
    char*       str_fn;
    char*       str_rn;
    Int4          num_fn;
    Int4          num_rn;

    if (ppp == NULL || count < 1)
        return;

    objects::CBioSource::TSubtype& subs = bio.SetSubtype();
    CRef<objects::CSubSource> sub;

    if (count == 1)
    {
        sub.Reset(new objects::CSubSource);
        sub->SetSubtype(33);
        sub->SetName(ppp->fwd_seq);
        subs.push_back(sub);

        sub.Reset(new objects::CSubSource);
        sub->SetSubtype(34);
        sub->SetName(ppp->rev_seq);
        subs.push_back(sub);

        if(ppp->fwd_name != NULL && ppp->fwd_name[0] != '\0')
        {
            sub.Reset(new objects::CSubSource);
            sub->SetSubtype(35);
            sub->SetName(ppp->fwd_name);
            subs.push_back(sub);
        }

        if(ppp->rev_name != NULL && ppp->rev_name[0] != '\0')
        {
            sub.Reset(new objects::CSubSource);
            sub->SetSubtype(36);
            sub->SetName(ppp->rev_name);
            subs.push_back(sub);
        }
        return;
    }

    size_t len_fs = 2,
           len_rs = 2,
           len_fn = 0,
           len_rn = 0;
    num_fn = 0;
    num_rn = 0;
    for(tppp = ppp; tppp != NULL; tppp = tppp->next)
    {
        len_fs += (StringLen(tppp->fwd_seq) + 1);
        len_rs += (StringLen(tppp->rev_seq) + 1);
        if(tppp->fwd_name != NULL && tppp->fwd_name[0] != '\0')
        {
            len_fn += (StringLen(tppp->fwd_name) + 1);
            num_fn++;
        }
        if(tppp->rev_name != NULL && tppp->rev_name[0] != '\0')
        {
            len_rn += (StringLen(tppp->rev_name) + 1);
            num_rn++;
        }
    }

    str_fs = (char*) MemNew(len_fs);
    str_rs = (char*) MemNew(len_rs);
    str_fn = (len_fn == 0) ? NULL : (char*) MemNew(len_fn + count -
                                                     num_fn + 2);
    str_rn = (len_rn == 0) ? NULL : (char*) MemNew(len_rn + count -
                                                     num_rn + 2);

    for(tppp = ppp; tppp != NULL; tppp = tppp->next)
    {
        StringCat(str_fs, ",");
        StringCat(str_fs, tppp->fwd_seq);
        StringCat(str_rs, ",");
        StringCat(str_rs, tppp->rev_seq);
        if(str_fn != NULL)
        {
            StringCat(str_fn, ",");
            if(tppp->fwd_name != NULL && tppp->fwd_name[0] != '\0')
                StringCat(str_fn, tppp->fwd_name);
        }
        if(str_rn != NULL)
        {
            StringCat(str_rn, ",");
            if(tppp->rev_name != NULL && tppp->rev_name[0] != '\0')
                StringCat(str_rn, tppp->rev_name);
        }
    }

    str_fs[0] = '(';
    StringCat(str_fs, ")");

    sub.Reset(new objects::CSubSource);
    sub->SetSubtype(33);
    sub->SetName(str_fs);
    subs.push_back(sub);

    str_rs[0] = '(';
    StringCat(str_rs, ")");

    sub.Reset(new objects::CSubSource);
    sub->SetSubtype(34);
    sub->SetName(str_rs);
    subs.push_back(sub);

    if(str_fn != NULL)
    {
        str_fn[0] = '(';
        StringCat(str_fn, ")");

        sub.Reset(new objects::CSubSource);
        sub->SetSubtype(35);
        sub->SetName(str_fn);
        subs.push_back(sub);
    }

    if(str_rn != NULL)
    {
        str_rn[0] = '(';
        StringCat(str_rn, ")");

        sub.Reset(new objects::CSubSource);
        sub->SetSubtype(36);
        sub->SetName(str_rn);
        subs.push_back(sub);
    }
}

/**********************************************************/
static void PcrPrimersFree(PcrPrimersPtr ppp)
{
    PcrPrimersPtr next;

    for(; ppp != NULL; ppp = next)
    {
        next = ppp->next;
        if(ppp->fwd_name != NULL)
            MemFree(ppp->fwd_name);
        if(ppp->fwd_seq != NULL)
            MemFree(ppp->fwd_seq);
        if(ppp->rev_name != NULL)
            MemFree(ppp->rev_name);
        if(ppp->rev_seq != NULL)
            MemFree(ppp->rev_seq);
        MemFree(ppp);
    }
}

/**********************************************************/
static bool ParsePcrPrimers(SourceFeatBlkPtr sfbp)
{
    PcrPrimersPtr ppp;
    PcrPrimersPtr tppp;

    char*       p;
    char*       q;
    char*       r;
    char*       s;
    bool          comma;
    bool          bad_start;
    bool          empty;
    Char          ch;
    Int4          count;
    Int4          prev;                 /* 1 = fwd_name, 2 = fwd_seq,
                                           3 = rev_name, 4 = rev_seq */

    bool got_problem = false;
    for(ppp = NULL; sfbp != NULL; sfbp = sfbp->next)
    {
        if (sfbp->quals.empty() || sfbp->bio_src.Empty())
            continue;

        count = 0;
        ITERATE(TQualVector, cur, sfbp->quals)
        {
            if((*cur)->GetQual() != "PCR_primers" ||
               !(*cur)->IsSetVal() || (*cur)->GetVal().empty())
                continue;

            count++;
            if(ppp == NULL)
            {
                ppp = (PcrPrimersPtr) MemNew(sizeof(PcrPrimers));
                tppp = ppp;
            }
            else
            {
                tppp->next = (PcrPrimersPtr) MemNew(sizeof(PcrPrimers));
                tppp = tppp->next;
            }

            prev = 0;
            std::vector<Char> val_buf((*cur)->GetVal().begin(), (*cur)->GetVal().end());
            val_buf.push_back(0);

            for(comma = false, bad_start = false, p = &val_buf[0]; *p != '\0';)
            {
                q = CheckPcrPrimersTag(p);
                if(q == NULL)
                {
                    if (p != &val_buf[0])
                    {
                        p++;
                        continue;
                    }
                    bad_start = true;
                    break;
                }

                if(*q == ' ')
                    q++;
                for(r = q;;)
                {
                    r = StringChr(r, ',');
                    if(r == NULL)
                        break;
                    if(*++r == ' ')
                        r++;
                    if(CheckPcrPrimersTag(r) != NULL)
                        break;
                }
                if(r != NULL)
                {
                    r--;
                    if(*r == ' ')
                        r--;
                    if(r > q && *(r - 1) == ' ')
                        r--;
                    ch = *r;
                    *r = '\0';
                }

                if(StringChr(q, ',') != NULL)
                    comma = true;

                empty = false;
                if(q == NULL || *q == '\0')
                    empty = true;
                else if(StringNCmp(p, "fwd_name", 8) == 0)
                {
                    if(prev == 1)
                        prev = -2;
                    else if(prev > 2 && prev < 5)
                        prev = -1;
                    else
                    {
                        if(tppp->fwd_name == NULL)
                            tppp->fwd_name = StringSave(q);
                        else
                        {
                            s = (char*) MemNew(StringLen(tppp->fwd_name) +
                                                 StringLen(q) + 2);
                            StringCpy(s, tppp->fwd_name);
                            StringCat(s, ":");
                            StringCat(s, q);
                            MemFree(tppp->fwd_name);
                            tppp->fwd_name = s;
                        }
                        prev = 1;
                    }
                }
                else if(StringNCmp(p, "fwd_seq", 7) == 0)
                {
                    if(prev > 2 && prev < 5)
                        prev = -1;
                    else
                    {
                        if(tppp->fwd_seq == NULL)
                            tppp->fwd_seq = StringSave(q);
                        else
                        {
                            s = (char*) MemNew(StringLen(tppp->fwd_seq) +
                                                 StringLen(q) + 2);
                            StringCpy(s, tppp->fwd_seq);
                            StringCat(s, ":");
                            StringCat(s, q);
                            MemFree(tppp->fwd_seq);
                            tppp->fwd_seq = s;
                            if(prev != 1)
                            {
                                if(tppp->fwd_name == NULL)
                                    tppp->fwd_name = StringSave(":");
                                else
                                {
                                    s = (char*) MemNew(StringLen(tppp->fwd_name) + 2);
                                    StringCpy(s, tppp->fwd_name);
                                    StringCat(s, ":");
                                    MemFree(tppp->fwd_name);
                                    tppp->fwd_name = s;
                                }
                            }
                        }
                        prev = 2;
                    }
                }
                else if(StringNCmp(p, "rev_name", 8) == 0)
                {
                    if(prev == 3 || prev == 1)
                        prev = -2;
                    else
                    {
                        if(tppp->rev_name == NULL)
                            tppp->rev_name = StringSave(q);
                        else
                        {
                            s = (char*) MemNew(StringLen(tppp->rev_name) +
                                                 StringLen(q) + 2);
                            StringCpy(s, tppp->rev_name);
                            StringCat(s, ":");
                            StringCat(s, q);
                            MemFree(tppp->rev_name);
                            tppp->rev_name = s;
                        }
                        prev = 3;
                    }
                }
                else
                {
                    if(prev == 1)
                        prev = -2;
                    else
                    {
                        if(tppp->rev_seq == NULL)
                            tppp->rev_seq = StringSave(q);
                        else
                        {
                            s = (char*) MemNew(StringLen(tppp->rev_seq) +
                                                 StringLen(q) + 2);
                            StringCpy(s, tppp->rev_seq);
                            StringCat(s, ":");
                            StringCat(s, q);
                            MemFree(tppp->rev_seq);
                            tppp->rev_seq = s;
                            if(prev != 3)
                            {
                                if(tppp->rev_name == NULL)
                                    tppp->rev_name = StringSave(":");
                                else
                                {
                                    s = (char*) MemNew(StringLen(tppp->rev_name) + 2);
                                    StringCpy(s, tppp->rev_name);
                                    StringCat(s, ":");
                                    MemFree(tppp->rev_name);
                                    tppp->rev_name = s;
                                }
                            }
                        }
                        prev = 4;
                    }
                }

                if(r == NULL)
                    break;

                *r++ = ch;

                if(comma || prev < 0 || empty)
                    break;

                if(ch == ' ')
                    r++;
                if(*r == ' ')
                    r++;
                p = r;
            }

            if(prev == 1 || prev == 3)
                prev = -2;

            if(bad_start)
            {
                ErrPostEx(SEV_REJECT, ERR_QUALIFIER_InvalidPCRprimer,
                          "Unknown text found at the beginning of /PCR_primers qualifier: \"%s\". Entry dropped.",
                          &val_buf[0]);
                got_problem = true;
                break;
            }

            if(comma)
            {
                ErrPostEx(SEV_REJECT, ERR_QUALIFIER_PCRprimerEmbeddedComma,
                          "Encountered embedded comma within /PCR_primers qualifier's field value: \"%s\". Entry dropped.",
                          &val_buf[0]);
                got_problem = true;
                break;
            }

            if(prev == -1)
            {
                ErrPostEx(SEV_REJECT, ERR_QUALIFIER_InvalidPCRprimer,
                          "Encountered incorrect order of \"forward\" and \"reversed\" sequences within /PCR_primers qualifier: \"%s\". Entry dropped.",
                          &val_buf[0]);
                got_problem = true;
                break;
            }

            if(prev == -2)
            {
                ErrPostEx(SEV_REJECT, ERR_QUALIFIER_MissingPCRprimerSeq,
                          "/PCR_primers qualifier \"%s\" is missing or has an empty required fwd_seq or rev_seq fields (or both). Entry dropped.",
                          &val_buf[0]);
                got_problem = true;
                break;
            }

            if(empty)
            {
                ErrPostEx(SEV_REJECT, ERR_QUALIFIER_InvalidPCRprimer,
                          "/PCR_primers qualifier \"%s\" has an empty field value. Entry dropped.",
                          &val_buf[0]);
                got_problem = true;
                break;
            }

            if(tppp->fwd_seq == NULL || tppp->fwd_seq[0] == '\0' ||
               tppp->rev_seq == NULL || tppp->rev_seq[0] == '\0')
            {
                ErrPostEx(SEV_REJECT, ERR_QUALIFIER_MissingPCRprimerSeq,
                          "/PCR_primers qualifier \"%s\" is missing or has an empty required fwd_seq or rev_seq fields (or both). Entry dropped.",
                          &val_buf[0]);
                got_problem = true;
                break;
            }
        }

        if (got_problem)
        {
            PcrPrimersFree(ppp);
            break;
        }

        PopulatePcrPrimers(*sfbp->bio_src, ppp, count);
        PcrPrimersFree(ppp);
        ppp = NULL;
    }

    if(sfbp == NULL)
        return true;
    return false;
}

/**********************************************************/
static void CheckCollectionDate(SourceFeatBlkPtr sfbp, Parser::ESource source)
{
    const char *Mmm[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
                         "Aug", "Sep", "Oct", "Nov", "Dec", NULL};
    const char **b;
    const char *q;

    char*    p;
    char*    r;
    char*    val;
    Int4       year;
    Int4       month;
    Int4       day;
    Int4       bad;
    Int4       num_slash;
    Int4       num_T;
    Int4       num_colon;
    Int4       num_Z;
    Int4       len;

    CTime time(CTime::eCurrent);
    objects::CDate_std date(time);

    for(; sfbp != NULL; sfbp = sfbp->next)
    {
        if (sfbp->quals.empty() || sfbp->bio_src.Empty())
            continue;

        ITERATE(TQualVector, cur, sfbp->quals)
        {
            bad = 0;
            if ((*cur)->GetQual() != "collection_date" ||
                !(*cur)->IsSetVal() || (*cur)->GetVal().empty())
                continue;

            val = (char *) (*cur)->GetVal().c_str();
            for(num_slash = 0, p = val; *p != '\0'; p++)
                if(*p == '/')
                    num_slash++;

            if(num_slash > 1)
            {
                p = StringSave(sfbp->location);
                if(p != NULL && StringLen(p) > 50)
                    p[50] = '\0';
                ErrPostEx(SEV_ERROR, ERR_SOURCE_InvalidCollectionDate,
                          "/collection_date \"%s\" for source feature at \"%s\" has too many components.",
                          val, (p == NULL) ? "unknown location" : p);
                if(p != NULL)
                    MemFree(p);
                continue;
            }

            for(val = (char *) (*cur)->GetVal().c_str();;)
            {
                r = StringChr(val, '/');
                if(r != NULL)
                    *r = '\0';

                len = StringLen(val);

                if(len == 4)
                {
                    for(q = val; *q == '0';)
                        q++;
                    for(p = (char*) q; *p != '\0'; p++)
                        if(*p < '0' || *p > '9')
                            break;
                    if(*p != '\0')
                        bad = 1;
                    else if (atoi(q) > date.GetYear())
                        bad = 3;
                }
                else if(len == 8)
                {
                    if(val[3] != '-')
                        bad = 1;
                    else
                    {
                        p = val;
                        p[3] = '\0';
                        if(source == Parser::ESource::DDBJ)
                        {
                            if(p[0] >= 'a' && p[0] <= 'z')
                                p[0] &= ~040;
                            if(p[1] >= 'A' && p[1] <= 'Z')
                                p[1] |= 040;
                            if(p[2] >= 'A' && p[2] <= 'Z')
                                p[2] |= 040;
                        }
                        for(b = Mmm, month = 1; *b != NULL; b++, month++)
                            if(StringCmp(*b, p) == 0)
                                break;
                        if(*b == NULL)
                            bad = 1;
                        p[3] = '-';
                    }
                    if(bad == 0)
                    {
                        for(q = val + 4; *q == '0';)
                            q++;
                        for(p = (char*) q; *p != '\0'; p++)
                            if(*p < '0' || *p > '9')
                                break;
                        if(*p != '\0')
                            bad = 1;
                        else
                        {
                            year = atoi(q);
                            if(year > date.GetYear() ||
                               (year == date.GetYear() && month > date.GetMonth()))
                                bad = 3;
                        }
                    }
                }
                else if(len == 11)
                {
                    if(val[2] != '-' || val[6] != '-')
                        bad = 1;
                    else
                    {
                        p = val;
                        val[2] = '\0';
                        val[6] = '\0';
                        if(p[0] < '0' || p[0] > '3' || p[1] < '0' || p[1] > '9')
                            bad = 1;
                        else
                        {
                            if(*p == '0')
                                p++;
                            day = atoi(p);
                            p = val + 3;
                            if(source == Parser::ESource::DDBJ)
                            {
                                if(p[0] >= 'a' && p[0] <= 'z')
                                    p[0] &= ~040;
                                if(p[1] >= 'A' && p[1] <= 'Z')
                                    p[1] |= 040;
                                if(p[2] >= 'A' && p[2] <= 'Z')
                                    p[2] |= 040;
                            }
                            for(b = Mmm, month = 1; *b != NULL; b++, month++)
                                if(StringCmp(*b, p) == 0)
                                    break;
                            if(*b == NULL)
                                bad = 1;
                            else
                            {
                                if(day < 1 || day > 31)
                                    bad = 2;
                                else if(month == 2 && day > 29)
                                    bad = 2;
                                else if((month == 4 || month == 6 || month == 9 || month == 11) && day > 30)
                                    bad = 2;
                            }
                        }
                        if(bad == 0)
                        {
                            for(q = val + 7; *q == '0';)
                                q++;
                            for(p = (char*) q; *p != '\0'; p++)
                                if(*p < '0' || *p > '9')
                                    break;
                            if(*p != '\0')
                                bad = 1;
                            else
                            {
                                year = atoi(q) - 1900;
                                if(year > date.GetYear() ||
                                   (year == date.GetYear() && month > date.GetMonth()) ||
                                   (year == date.GetYear() && month == date.GetMonth() && day > date.GetDay()))
                                    bad = 3;
                            }
                        }
                        val[2] = '-';
                        val[6] = '-';
                    }
                }
                else if(len == 7 || len == 10 || len == 14 || len == 17 ||
                        len == 20)
                {
                    num_T = 0;
                    num_Z = 0;
                    num_colon = 0;
                    for(p = val; *p != '\0'; p++)
                    {
                        if((*p < 'a' || *p > 'z') && (*p < 'A' || *p > 'Z') &&
                           (*p < '0' || *p > '9') && *p != '-' && *p != '/' &&
                           *p != ':')
                        {
                            bad = 3;
                            break;
                        }
                        if(*p == ':')
                            num_colon++;
                        else if(*p == 'T')
                            num_T++;
                        else if(*p == 'Z')
                            num_Z++;
                    }
                    if(len == 7 || len == 10)
                    {
                        if(num_T > 0)
                            bad = 4;
                        if(num_Z > 0)
                            bad = 5;
                        if(num_colon > 0)
                            bad = 6;
                    }
                    else
                    {
                        if(num_Z > 1)
                            bad = 5;
                        if(num_T > 1)
                            bad = 4;
                        if((len == 14 && num_colon > 0) ||
                           (len == 17 && num_colon > 1) ||
                           (len == 20 && num_colon > 2))
                            bad = 6;
                    }
                }
                else
                    bad = 8;

                if(bad == 0)
                {
                    if(r == NULL)
                        break;

                    *r = '/';
                    val = r + 1;
                    continue;
                }

                p = StringSave(sfbp->location);
                if(p != NULL && StringLen(p) > 50)
                    p[50] = '\0';
                if(bad == 1)
                    q = "is not of the format DD-Mmm-YYYY, Mmm-YYYY, or YYYY";
                else if(bad == 2)
                    q = "has an illegal day value for the stated month";
                else if(bad == 3)
                    q = "has invalid characters";
                else if(bad == 4)
                    q = "has too many time values";
                else if(bad == 5)
                    q = "has too many Zulu indicators";
                else if(bad == 6)
                    q = "has too many hour and minute delimiters";
                else
                    q = "has not yet occured";
                ErrPostEx(SEV_ERROR, ERR_SOURCE_InvalidCollectionDate,
                          "/collection_date \"%s\" for source feature at \"%s\" %s.",
                          val, (p == NULL) ? "unknown location" : p, q);
                if(p != NULL)
                    MemFree(p);

                if(r == NULL)
                    break;

                *r = '/';
                val = r + 1;
            }
        }
    }
}

/**********************************************************/
static bool CheckNeedSYNFocus(SourceFeatBlkPtr sfbp)
{
    const char **b;

    if(sfbp == NULL || sfbp->next == NULL)
        return false;

    for(; sfbp != NULL; sfbp = sfbp->next)
    {
        if(!sfbp->full)
            continue;

        for(b = special_orgs; *b != NULL; b++)
            if(StringICmp(*b, sfbp->name) == 0)
                break;

        if(*b != NULL)
            break;
    }

    if(sfbp != NULL)
        return false;
    return true;
}

/**********************************************************/
static void CheckMetagenome(objects::CBioSource& bio)
{
    if (!bio.IsSetOrg())
        return;

    bool metatax = false;
    bool metalin = false;

    if (bio.IsSetOrgname() && bio.GetOrgname().IsSetLineage() &&
        StringStr(bio.GetOrgname().GetLineage().c_str(), "metagenomes") != NULL)
        metalin = true;

    if (bio.GetOrg().IsSetTaxname() &&
        StringStr(bio.GetOrg().GetTaxname().c_str(), "metagenome") != NULL)
        metatax = true;

    if(!metalin && !metatax)
        return;

    const Char* taxname = bio.GetOrg().IsSetTaxname() ? bio.GetOrg().GetTaxname().c_str() : NULL;
    if (taxname == NULL || taxname[0] == 0)
        taxname = "unknown";

    if (metalin && metatax)
    {
        CRef<objects::CSubSource> sub(new objects::CSubSource);
        sub->SetSubtype(37);
        sub->SetName("");
        bio.SetSubtype().push_back(sub);
    }
    else if(!metalin)
        ErrPostEx(SEV_ERROR, ERR_ORGANISM_LineageLacksMetagenome,
                  "Organism name \"%s\" contains \"metagenome\" but the lineage lacks the \"metagenomes\" classification.",
                  taxname);
    else
        ErrPostEx(SEV_ERROR, ERR_ORGANISM_OrgNameLacksMetagenome,
                  "Lineage includes the \"metagenomes\" classification but organism name \"%s\" lacks \"metagenome\".",
                  taxname);
}

/**********************************************************/
static bool CheckSubmitterSeqidQuals(SourceFeatBlkPtr sfbp, char* acc)
{
    SourceFeatBlkPtr tsfbp;
    char*          ssid;
    Int4             count_feat;
    Int4             count_qual;

    if(sfbp == NULL)
        return(true);

    count_feat = 0;
    count_qual = 0;
    for(ssid = NULL, tsfbp = sfbp; tsfbp != NULL; tsfbp = tsfbp->next)
    {
        count_feat++;
        if(tsfbp->submitter_seqid == NULL)
            continue;

        count_qual++;
        if(tsfbp->submitter_seqid[0] == '\0')
        {
            ErrPostEx(SEV_REJECT, ERR_SOURCE_MultipleSubmitterSeqids,
                      "Multiple /submitter_seqid qualifiers were encountered within source feature at location \"%s\". Entry dropped.",
                      (tsfbp->location == NULL) ? "?empty?" : tsfbp->location);
            break;
        }

        if(ssid == NULL)
            ssid = tsfbp->submitter_seqid;
        else if(StringCmp(ssid, tsfbp->submitter_seqid) != 0)
        {
            ErrPostEx(SEV_REJECT, ERR_SOURCE_DifferentSubmitterSeqids,
                      "Different /submitter_seqid qualifiers were encountered amongst source features: \"%s\" and \"%s\" at least. Entry dropped.",
                      ssid, tsfbp->submitter_seqid);
            break;
        }
    }

    if(tsfbp != NULL)
        return(false);

    if(count_feat == count_qual)
        return(true);

    ErrPostEx(SEV_REJECT, ERR_SOURCE_LackingSubmitterSeqids,
              "One ore more source features are lacking /submitter_seqid qualifiers provided in others. Entry dropped.");
    return(false);
}

/**********************************************************/
void ParseSourceFeat(ParserPtr pp, DataBlkPtr dbp, TSeqIdList& seqids,
                     Int2 type, objects::CBioseq& bioseq, TSeqFeatList& seq_feats)
{
    SourceFeatBlkPtr sfbp;
    SourceFeatBlkPtr tsfbp;

    MinMaxPtr        mmp;
    IndexblkPtr      ibp;
    char*          res;
    char*          acc;
    char*          p;
    Int4             i;
    Int4             use_what = USE_ALL;
    bool             err;
    ErrSev           sev;
    bool             need_focus;
    bool             already;

    ibp = pp->entrylist[pp->curindx];
    acc = ibp->acnum;
    size_t len = ibp->bases;

    if(ibp->segnum < 2)
        pp->errstat = 0;

    sfbp = CollectSourceFeats(dbp, type);
    if(sfbp == NULL)
    {
        ErrPostEx(SEV_REJECT, ERR_SOURCE_FeatureMissing,
                  "Required source feature is missing. Entry dropped.");
        return;
    }

    RemoveSourceFeatSpaces(sfbp);
    CheckForExemption(sfbp);

    if(!CheckSourceFeatLocFuzz(sfbp))
    {
        SourceFeatBlkSetFree(sfbp);
        return;
    }

    res = CheckSourceFeatLocAccs(sfbp, acc);
    if(res != NULL)
    {
        ErrPostEx(SEV_REJECT, ERR_SOURCE_BadLocation,
                  "Source feature location points to another record: \"%s\". Entry dropped.",
                  res);
        SourceFeatBlkSetFree(sfbp);
        return;
    }

    if(!SourceFeatStructFillIn(ibp, sfbp, use_what))
    {
        ErrPostEx(SEV_REJECT, ERR_SOURCE_MultipleMolTypes,
                  "Multiple /mol_type qualifiers were encountered within source feature. Entry dropped.");
        SourceFeatBlkSetFree(sfbp);
        return;
    }

    if(ibp->submitter_seqid && !CheckSubmitterSeqidQuals(sfbp, acc))
    {
        MemFree(ibp->submitter_seqid);
        ibp->submitter_seqid = NULL;
        SourceFeatBlkSetFree(sfbp);
        return;
    }

    if(!CheckMoltypeConsistency(sfbp, &ibp->moltype))
    {
        ErrPostEx(SEV_REJECT, ERR_SOURCE_InconsistentMolType,
                  "Inconsistent /mol_type qualifiers were encountered. Entry dropped.");
        SourceFeatBlkSetFree(sfbp);
        return;
    }

    res = CheckSourceFeatFocusAndTransposon(sfbp);
    if(res != NULL)
    {
        ErrPostEx(SEV_REJECT, ERR_SOURCE_FocusAndTransposonNotAllowed,
                  "/transposon (or /insertion_seq) qualifiers should not be used in conjunction with /focus. Source feature at \"%s\". Entry dropped.",
                  res);
        SourceFeatBlkSetFree(sfbp);
        return;
    }

    res = CheckSourceFeatOrgs(sfbp, &i);
    if(res != NULL)
    {
        if(i == 1)
        {
            ErrPostEx(SEV_REJECT, ERR_SOURCE_NoOrganismQual,
                      "/organism qualifier contains only organell/genome name. No genus/species present. Source feature at \"%s\". Entry dropped.",
                      res);
        }
        else
        {
            ErrPostEx(SEV_REJECT, ERR_SOURCE_OrganismIncomplete,
                      "Required /organism qualifier is containing genome info only at \"%s\". Entry dropped.",
                      res);
        }
        SourceFeatBlkSetFree(sfbp);
        return;
    }

    CompareDescrFeatSources(sfbp, bioseq);

    CreateRawBioSources(pp, sfbp, use_what);

    if(!CheckSourceLineage(sfbp, pp->source, ibp->is_pat))
    {
        SourceFeatBlkSetFree(sfbp);
        return;
    }

    PropogateSuppliedLineage(bioseq, sfbp, pp->taxserver);

    mmp = (MinMaxPtr) MemNew(sizeof(MinMax));
    mmp->orgname = NULL;
    mmp->min = 0;
    mmp->max = 0;
    mmp->skip = false;
    i = CheckSourceFeatCoverage(sfbp, mmp, len);
    if(i != 0)
    {
        if(i == 1)
        {
            ErrPostEx(SEV_REJECT, ERR_SOURCE_IncompleteCoverage,
                      "Supplied source features do not span every base of the sequence. Entry dropped.");
        }
        else
        {
            ErrPostEx(SEV_REJECT, ERR_SOURCE_ExcessCoverage,
                      "Sequence is spanned by too many source features. Entry dropped.");
        }
        SourceFeatBlkSetFree(sfbp);
        MinMaxFree(mmp);
        return;
    }

    if(!CheckForENV(sfbp, ibp, pp->source))
    {
        SourceFeatBlkSetFree(sfbp);
        MinMaxFree(mmp);
        return;
    }

    if(!CheckSYNTGNDivision(sfbp, ibp->division))
    {
        SourceFeatBlkSetFree(sfbp);
        MinMaxFree(mmp);
        return;
    }

    if(pp->source == Parser::ESource::EMBL)
        need_focus = CheckNeedSYNFocus(sfbp);
    else
        need_focus = true;

    already = false;
    i = CheckTransgenicSourceFeats(sfbp);
    if(i == 5)
    {
        if(pp->source == Parser::ESource::DDBJ || pp->source == Parser::ESource::EMBL)
            sev = SEV_WARNING;
        else
            sev = SEV_ERROR;
        ErrPostEx(sev, ERR_SOURCE_TransSingleOrgName,
                  "Use of /transgenic requires at least two source features with differences among /organism, /strain, /organelle, and /isolate, between the host and foreign organisms.");
    }
    else if(i > 0)
    {
        sev = SEV_REJECT;
        if(i == 1)
        {
            ErrPostEx(sev, ERR_SOURCE_TransgenicTooShort,
                      "Source feature with /transgenic qualifier does not span the entire sequence. Entry dropped.");
        }
        else if(i == 2)
        {
            ErrPostEx(sev, ERR_SOURCE_FocusAndTransgenicQuals,
                      "Both /focus and /transgenic qualifiers exist; these quals are mutually exclusive. Entry dropped.");
        }
        else if(i == 3)
        {
            ErrPostEx(sev, ERR_SOURCE_MultipleTransgenicQuals,
                      "Multiple source features have /transgenic qualifiers. Entry dropped.");
        }
        else
        {
            already = true;
            if(!need_focus)
                sev = SEV_ERROR;
            ErrPostEx(sev, ERR_SOURCE_FocusQualMissing,
                      "Multiple organism names exist, but no source feature has a /focus qualifier.%s",
                      (sev == SEV_ERROR) ? "" : " Entry dropped.");
        }

        if(sev == SEV_REJECT)
        {
            SourceFeatBlkSetFree(sfbp);
            MinMaxFree(mmp);
            return;
        }
    }

    res = CheckWholeSourcesVersusFocused(sfbp);
    if(res != NULL)
    {
        ErrPostEx(SEV_REJECT, ERR_SOURCE_FocusQualNotFullLength,
                  "/focus qualifier should be used for the full-length source feature, not on source feature at \"%s\".",
                  res);
        SourceFeatBlkSetFree(sfbp);
        MinMaxFree(mmp);
        return;
    }
    i = CheckFocusInOrgs(sfbp, len, &pp->errstat);
    if(pp->errstat != 0 && (ibp->segnum == 0 || pp->errstat == ibp->segtotal))
        i = 1;
    if(i > 0)
    {
        sev = SEV_REJECT;
        if(i == 1)
        {
            ErrPostEx(sev, ERR_SOURCE_FocusQualNotNeeded,
                      "/focus qualifier present, but only one organism name exists. Entry dropped.");
        }
        else if(i == 2)
        {
            ErrPostEx(sev, ERR_SOURCE_MultipleOrganismWithFocus,
                      "/focus qualifiers exist on source features with differing organism names. Entry dropped.");
        }
        else
        {
            if(!need_focus)
                sev = SEV_ERROR;
            if(!already)
                ErrPostEx(sev, ERR_SOURCE_FocusQualMissing,
                          "Multiple organism names exist, but no source feature has a /focus qualifier.%s",
                          (sev == SEV_ERROR) ? "" : " Entry dropped.");
        }

        if(sev == SEV_REJECT)
        {
            SourceFeatBlkSetFree(sfbp);
            MinMaxFree(mmp);
            return;
        }
    }
    res = CheckSourceOverlap(mmp->next, len);
    MinMaxFree(mmp);
    if(res != NULL)
    {
        ErrPostEx(SEV_REJECT, ERR_SOURCE_MultiOrgOverlap,
                  "Overlapping source features have different organism names %s. Entry dropped.",
                  res);
        SourceFeatBlkSetFree(sfbp);
        MemFree(res);
        return;
    }

    res = CheckForUnusualFullLengthOrgs(sfbp);
    if(res != NULL)
    {
        ErrPostEx(SEV_WARNING, ERR_SOURCE_UnusualOrgName,
                  "Unusual organism name \"%s\" encountered for full-length source feature.",
                  res);
    }

    for(tsfbp = sfbp, i = 0; tsfbp != NULL; tsfbp = tsfbp->next)
        i++;
    if(i > BIOSOURCES_THRESHOLD)
    {
        ErrPostEx(SEV_WARNING, ERR_SOURCE_ManySourceFeats,
                  "This record has more than %d source features.",
                  BIOSOURCES_THRESHOLD);
    }

    if(!ParsePcrPrimers(sfbp))
    {
        SourceFeatBlkSetFree(sfbp);
        return;
    }

    CheckCollectionDate(sfbp, pp->source);

    sfbp = PickTheDescrSource(sfbp);
    if(sfbp == NULL || !UpdateRawBioSource(sfbp, pp->source, ibp, pp->taxserver))
    {
        SourceFeatBlkSetFree(sfbp);
        return;
    }

    if (sfbp->focus)
        sfbp->bio_src->SetIs_focus();
    else
        sfbp->bio_src->ResetIs_focus();


    for (tsfbp = sfbp; tsfbp != NULL; tsfbp = tsfbp->next)
    {
        CheckMetagenome(*tsfbp->bio_src);

        CRef<objects::CSeq_feat> feat(new objects::CSeq_feat);
        feat->SetData().SetBiosrc(*tsfbp->bio_src);

        if(pp->buf != NULL)
            MemFree(pp->buf);
        pp->buf = NULL;

        GetSeqLocation(*feat, tsfbp->location, seqids, &err,
                       pp, (char*) "source");

        if(err)
        {
            ErrPostEx(SEV_ERROR, ERR_FEATURE_Dropped,
                      "/source|%s| range check detects problems. Entry dropped.",
                      tsfbp->location);
            break;
        }

        if (!tsfbp->quals.empty())
        {
            p = GetTheQualValue(tsfbp->quals, "evidence");
            if(p != NULL)
            {
                if(StringICmp(p, "experimental") == 0)
                    feat->SetExp_ev(objects::CSeq_feat::eExp_ev_experimental);
                else if(StringICmp(p, "not_experimental") == 0)
                    feat->SetExp_ev(objects::CSeq_feat::eExp_ev_not_experimental);
                MemFree(p);
            }
        }

        seq_feats.push_back(feat);
    }

    SourceFeatBlkSetFree(sfbp);

    if(tsfbp != NULL)
        seq_feats.clear();
}

END_NCBI_SCOPE
