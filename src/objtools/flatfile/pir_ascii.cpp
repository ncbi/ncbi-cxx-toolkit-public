/* pir_ascii.c
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
 * File Name:  pir_ascii.c
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *
 */

// LCOV_EXCL_START
// Excluded per Mark's request on 12/14/2016
#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <objects/seq/Seq_inst.hpp>
#include <objmgr/scope.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqblock/PIR_block.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Affil.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seq/Pubdesc.hpp>

#include "index.h"
#include "pir_index.h"

#include <objtools/flatfile/flatdefn.h>
#include "ftanet.h"
#include <objtools/flatfile/flatfile_parser.hpp>

#include "ftaerr.hpp"
#include "asci_blk.h"
#include "utilref.h"
#include "add.h"
#include "pir_ascii.h"
#include "utilfun.h"
#include "xutils.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "pir_ascii.cpp"


BEGIN_NCBI_SCOPE

const char *dbp_tag[] = {
    "ENTRY",
    "TITLE",
    "ALTERNATE_NAMES",
    "CONTAINS",
    "ORGANISM",
    "DATE",
    "ACCESSIONS",
    "REFERENCE",
    "COMMENT",
    "COMPLEX",
    "GENETICS",
    "FUNCTION",
    "CLASSIFICATION",
    "KEYWORDS",
    "FEATURE",
    "SUMMARY",
    "SEQUENCE",
    "///",
    NULL
};

const char *sub_tag_ENTRY[] = {
    "#*",
    "#type",
    NULL
};

const char *sub_tag_TITLE[] = {
    "#*",
    NULL
};

const char *sub_tag_ALTERNATE_NAMES[] = {
    "#*",
    NULL
};

const char *sub_tag_CONTAINS[] = {
    "#*",
    NULL
};

const char *sub_tag_ORGANISM[] = {
    "#*",
    "#formal_name",
    "#common_name",
    "#variety",
    "#note",
    NULL
};

const char *sub_tag_DATE[] = {
    "#*",
    "#sequence_revision",
    "#text_change",
    NULL
};

const char *sub_tag_ACCESSIONS[] = {
    "#*",
    NULL
};

const char *sub_tag_REFERENCE[] = {
    "#*",
    "#authors",
    "#title",
    "#journal",
    "#book",
    "#citation",
    "#submission",
    "#description",
    "#contents",
    "#note",
    "#cross-references",
    "#accession",
    NULL
};

const char *sub_tag_COMMENT[] = {
    "#*",
    NULL
};

const char *sub_tag_COMPLEX[] = {
    "#*",
    NULL
};

const char *sub_tag_GENETICS[] = {
    "#*",
    "#gene",
    "#map_position",
    "#genome",
    "#genetic_code",
    "#start_codon",
    "#introns",
    "#note",
    "#status",
    "#mobile_element",
    NULL
};

const char *sub_tag_FUNCTION[] = {
    "#*",
    "#description",
    "#note",
    "#pathway",
    NULL
};

const char *sub_tag_CLASSIFICATION[] = {
    "#*",
    "#superfamily",
    NULL
};

const char *sub_tag_KEYWORDS[] = {
    "#*",
    NULL
};

const char *sub_tag_FEATURES[] = {
    "#*",
    "#active_site",
    "#binding_site",
    "#cleavage_site",
    "#inhibitory_site",
    "#modified_site",
    "#disulfide_bonds",
    "#thiolester_bonds",
    "#cross-link",
    "#domain",
    "#duplication",
    "#peptide",
    "#protein",
    "#region",
    "#status",
    "#label",
    NULL
};

const char *sub_tag_SUMMARY[] = {
    "#*",
    "#length",
    "#molecular-weight",
    "#checksum",
    NULL
};

const char *sub_tag_SEQUENCE[] = {
    "#*",
    NULL
};

const char *sub_tag_END[] = {
    "#*",
    NULL
};

const char **sub_tag[] = {
    sub_tag_ENTRY,
    sub_tag_TITLE,
    sub_tag_ALTERNATE_NAMES,
    sub_tag_CONTAINS,
    sub_tag_ORGANISM,
    sub_tag_DATE,
    sub_tag_ACCESSIONS,
    sub_tag_REFERENCE,
    sub_tag_COMMENT,
    sub_tag_COMPLEX,
    sub_tag_GENETICS,
    sub_tag_FUNCTION,
    sub_tag_CLASSIFICATION,
    sub_tag_KEYWORDS,
    sub_tag_FEATURES,
    sub_tag_SUMMARY,
    sub_tag_SEQUENCE,
    sub_tag_END,
};

DataBlkPtr sub_ind_ENTRY[MAXTAG];
DataBlkPtr sub_ind_TITLE[MAXTAG];
DataBlkPtr sub_ind_ALTERNATE_NAMES[MAXTAG];
DataBlkPtr sub_ind_CONTAINS[MAXTAG];
DataBlkPtr sub_ind_ORGANISM[MAXTAG];
DataBlkPtr sub_ind_DATE[MAXTAG];
DataBlkPtr sub_ind_ACCESSIONS[MAXTAG];
DataBlkPtr sub_ind_REFERENCE[MAXTAG];
DataBlkPtr sub_ind_COMMENT[MAXTAG];
DataBlkPtr sub_ind_COMPLEX[MAXTAG];
DataBlkPtr sub_ind_GENETICS[MAXTAG];
DataBlkPtr sub_ind_FUNCTION[MAXTAG];
DataBlkPtr sub_ind_CLASSIFICATION[MAXTAG];
DataBlkPtr sub_ind_KEYWORDS[MAXTAG];
DataBlkPtr sub_ind_FEATURE[MAXTAG];
DataBlkPtr sub_ind_SUMMARY[MAXTAG];
DataBlkPtr sub_ind_SEQUENCE[MAXTAG];
DataBlkPtr sub_ind_END[MAXTAG];

DataBlkPtr* sub_ind[] = {
    sub_ind_ENTRY,
    sub_ind_TITLE,
    sub_ind_ALTERNATE_NAMES,
    sub_ind_CONTAINS,
    sub_ind_ORGANISM,
    sub_ind_DATE,
    sub_ind_ACCESSIONS,
    sub_ind_REFERENCE,
    sub_ind_COMMENT,
    sub_ind_COMPLEX,
    sub_ind_GENETICS,
    sub_ind_FUNCTION,
    sub_ind_CLASSIFICATION,
    sub_ind_KEYWORDS,
    sub_ind_FEATURE,
    sub_ind_SUMMARY,
    sub_ind_SEQUENCE,
    sub_ind_END,
};

#define MAXSTR     322
#define MAXDBP     3002
#define MAXMAX     70000

static DataBlk         db[MAXDBP];
static DataBlk*    subdb[MAXDBP*MAXTAG];

static int             i_dbp;
static int             i_subdbp;

#define OTHER_MEDIUM    255

const char *ArrayOrganelle[] = {
    "mitochondrion",
    "chloroplast",
    "kinetoplast",
    "chromoplast",
    "cyanelle",
    "plastid",
    "nucleomorph",
    NULL
};

/* peptide and protein will be replaced by product or domain
 */
const char *PirFeatInput[] = {
    "#active_site",
    "#binding_site",
    "#cleavage_site",
    "#inhibitory_site",
    "#modified_site",
    "#disulfide_bonds",
    "#thiolester_bonds",
    "#cross-link",
    "#domain",
    "#duplication",
    "#peptide",
    "#protein",
    "#region",
    "#product",
    NULL
};

/* number of entries in arry below must be the same or more than in
 * above one
 */
PirFeatType PirFeat[] = {
    { 12, objects::CSeqFeatData::e_Site, 1, NULL },
    { 13, objects::CSeqFeatData::e_Site, 2, NULL },
    { 14, objects::CSeqFeatData::e_Site, 3, NULL },
    { 16, objects::CSeqFeatData::e_Site, 4, NULL },
    { 14, objects::CSeqFeatData::e_Site, 5, NULL },
    { 16, objects::CSeqFeatData::e_Bond, 1, NULL },
    { 17, objects::CSeqFeatData::e_Bond, 2, NULL },
    { 12, objects::CSeqFeatData::e_Bond, 3, NULL },
    { 7, objects::CSeqFeatData::e_Region, -1, "domain" },
    { 12, objects::CSeqFeatData::e_Region, -1, "duplication" },
    { 8, objects::CSeqFeatData::e_Region, -1, "peptide" },
    { 8, objects::CSeqFeatData::e_Region, -1, "protein" },
    { 7, objects::CSeqFeatData::e_Region, -1, "region" },
    { 8, objects::CSeqFeatData::e_Region, -1, "product" }
};

static const char *month_name[] = {
    "Ill", "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
    "JUL", "AUG", "SEP", "OCT", "NOV", "DEC", NULL
};

/**********************************************************/
static Int2 find_organelle(const Char* text)
{
    const Char **b;
    Int2       i;

    if(text == NULL)
        return(-1);

    for(i = 0, b = ArrayOrganelle; *b != NULL; b++, i++)
        if(StringNICmp(text, *b, StringLen(*b)) == 0)
            break;
    if(*b == NULL)
        return(-1);
    return(i);
}

/**********************************************************/
static const Char* strip_organelle(const Char* text)
{
    Int2 i;

    i = find_organelle(text);
    if(i != -1)
    {
        text += StringLen(ArrayOrganelle[i]);
        while(*text == ' ')
            text++;
    }

    return(text);
}

/**********************************************************/
static Uint1 GetPirGenome(DataBlkPtr** sub_ind)
{
    DataBlkPtr dbp;
    Int4       gmod;

    dbp = sub_ind[ParFlatPIR_ORGANISM][ORGANISM_formal_name];
    if(dbp == NULL)
        return(0);

    gmod = find_organelle(dbp->mOffset);
    if(gmod == 0)
        return(5);                      /* mitochondrion */
    if(gmod == 1)
        return(2);                      /* chloroplast */
    if(gmod == 2)
        return(4);                      /* kinetoplast */
    if(gmod == 3)
        return(3);                      /* chromoplast */
    if(gmod == 4)
        return(12);                     /* cyanelle */
    return(0);                          /* plastid (5), nucleomorph (6)
                                           and all the others */
}

/**********************************************************/
static bool check_pir_entry(DataBlkPtr* ind)
{
    if(ind[ParFlatPIR_ENTRY] == NULL)
        ErrPostEx(SEV_ERROR, ERR_ENTRY_NumKeywordBlk, "No ENTRY block found");
    else if(ind[ParFlatPIR_ENTRY]->mpNext != NULL)
        ErrPostEx(SEV_ERROR, ERR_ENTRY_NumKeywordBlk,
                  "More than one ENTRY block found");
    else if(ind[ParFlatPIR_TITLE] == NULL)
        ErrPostEx(SEV_ERROR, ERR_TITLE_NumKeywordBlk, "No TITLE block found");
    else if(ind[ParFlatPIR_TITLE]->mpNext != NULL)
        ErrPostEx(SEV_ERROR, ERR_TITLE_NumKeywordBlk,
                  "More than one TITLE block found");
    else if(ind[ParFlatPIR_DATE] == NULL)
        ErrPostEx(SEV_ERROR, ERR_DATE_NumKeywordBlk, "No DATE block found");
    else if(ind[ParFlatPIR_DATE]->mpNext != NULL)
        ErrPostEx(SEV_ERROR, ERR_DATE_NumKeywordBlk,
                  "More than one DATE block found");
    else if(ind[ParFlatPIR_REFERENCE] == NULL)
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_NumKeywordBlk,
                  "No reference block found");
    else if(ind[ParFlatPIR_SUMMARY] == NULL)
        ErrPostEx(SEV_ERROR, ERR_SUMMARY_NumKeywordBlk,
                  "No SUMMARY block found");
    else if(ind[ParFlatPIR_SUMMARY]->mpNext != NULL)
        ErrPostEx(SEV_ERROR, ERR_SUMMARY_NumKeywordBlk,
                  "More than one SUMMARY block found");
    else if(ind[ParFlatPIR_SEQUENCE] == NULL)
        ErrPostEx(SEV_ERROR, ERR_SEQUENCE_NumKeywordBlk,
                  "No SEQUENCE block found");
    else if(ind[ParFlatPIR_SEQUENCE]->mpNext != NULL)
        ErrPostEx(SEV_ERROR, ERR_SEQUENCE_NumKeywordBlk,
                  "More than one SEQUENCE block found");
    else if(ind[ParFlatPIR_END] == NULL)
        ErrPostEx(SEV_ERROR, ERR_SEQUENCE_NumKeywordBlk,
                  "No END_OF_SEQUENCE (///) found");
    else
        return true;
    return false;
}

/**********************************************************/
static CRef<objects::CSeq_entry> create_entry(DataBlkPtr* ind)
{
    /* create the entry framework
     */
    CRef<objects::CBioseq> bioseq(new objects::CBioseq);

    bioseq->SetInst().SetRepr(objects::CSeq_inst::eRepr_raw);
    bioseq->SetInst().SetMol(objects::CSeq_inst::eMol_aa);

    CRef<objects::CSeq_id> id(MakeLocusSeqId(ind[ParFlatPIR_ENTRY]->mOffset, objects::CSeq_id::e_Pir));
    if (id.NotEmpty())
        bioseq->SetId().push_back(id);

    CRef<objects::CSeq_entry> entry(new objects::CSeq_entry);
    entry->SetSeq(*bioseq);

    GetScope().AddBioseq(*bioseq);

    return entry;
}

/**********************************************************
 *
 *   static bool get_seq_data(ind, sub_ind, bsp, seqconv,
 *                               seq_data_type):
 *
 *      Modified from GetSeqData (Hsui-Chuan).
 *
 *                                              6-9-93
 *
 **********************************************************/
static bool get_seq_data(DataBlkPtr* ind, DataBlkPtr** sub_ind,
                         objects::CBioseq& bioseq, unsigned char* seqconv,
                         Uint1 seq_data_type)
{
    Uint4        seqlen;

    char*      seqptr = NULL;
    char*      endptr;
    char*      bptr = NULL;
    DataBlkPtr   dbp;
    Int4         i;

    dbp = sub_ind[ParFlatPIR_SUMMARY][1];
    if(dbp != NULL)
        bptr = dbp->mOffset;

    if(bptr == NULL)
        return false;

    bioseq.SetInst().SetLength(atol(bptr));

    seqlen = 0;
    seqptr = ind[ParFlatPIR_SEQUENCE]->mOffset;
    size_t len = ind[ParFlatPIR_SEQUENCE]->len;
    endptr = seqptr + len;

    /* the sequence data will be located in next line of nodetype
     */
    while(*seqptr != '\n')
        seqptr++;

    while(isalpha(*seqptr) == 0)       /* skip leading blanks and digits */
        seqptr++;

    std::vector<char> buf;
    for (i = 1; seqptr != endptr;)
    {
        i = ScanSequence(false, &seqptr, buf, seqconv, 'X', NULL);
        if(i == 0)
            break;
        seqlen += i;

        while(isalpha(*seqptr) == 0 && seqptr != endptr)
            seqptr++;
    }

    if(i == 0)
    {
        ErrPostEx(SEV_REJECT, ERR_SEQUENCE_BadData,
                  "Bad sequence data. Entry dropped.");
        return false;
    }

    /* an error occurred
     */
    if(seqlen != bioseq.GetInst().GetLength())
    {
        ErrPostEx(SEV_WARNING, ERR_SEQUENCE_SeqLenNotEq,
                  "Measured seqlen [%ld] != given [%ld]",
                  (long int)seqlen, (long int)bioseq.GetInst().GetLength());
    }

    bioseq.SetInst().SetSeq_data().Assign(objects::CSeq_data(buf, static_cast<objects::CSeq_data::E_Choice>(seq_data_type)));
    return true;
}

/**********************************************************/
static char* join_subind(DataBlkPtr ind, DataBlkPtr* sub_ind)
{
    char* str;
    char* add;
    char* tag;
    char* offset;
    Int2    l;
    int    type;

    type = ind->mType;
    size_t len = strlen(sub_ind[0]->mOffset) + 1;
    for(l = 1; sub_tag[type][l] != NULL; l++)
    {
        if(sub_ind[l] == NULL)
            continue;

        len += strlen(sub_tag[type][l]) + 1;
        len += strlen(sub_ind[l]->mOffset) + 1;
    }
    str = (char*) MemNew(len);
    add = tata_save(sub_ind[0]->mOffset);
    StringCpy(str, add);
    MemFree(add);
    for(l = 1; sub_tag[type][l] != NULL; l++)
    {
        if(sub_ind[l] == NULL)
            continue;

        tag = tata_save((char*) sub_tag[type][l]);
        StringCat(str, " ");
        StringCat(str, tag);
        MemFree(tag);
        offset = sub_ind[l]->mOffset;
        add = tata_save(offset);
        StringCat(str, " ");
        StringCat(str, add);
        MemFree(add);
    }

    add = tata_save(str);
    StringCpy(str, add);
    MemFree(add);
    return(str);
}

/**********************************************************
 *
 *   ValNodePtr split_str(vnp, instr):
 *
 *      Split string, instr by "\", then put into a link
 *   list vnp.
 *      New format PIR change "\" to ";".
 *
 *                                              11-17-93
 *
 **********************************************************/
static void split_str(TKeywordList& words, char* instr)
{
    char* ptr;
    char* ptr2;

    if(instr == NULL)
        return;

    ptr = instr;
    while(*ptr != '\0')
    {
        ptr2 = StringChr(ptr, ';');

        std::string str;
        if(ptr2 != NULL)
        {
            str.assign(ptr, ptr2);

            ptr = ptr2;
            while(*ptr == ';' || *ptr == ' ')
                ptr++;
        }
        else
        {
            str = NStr::Sanitize(ptr);
            *ptr = '\0';
        }

        words.push_back(str);
    }
}

/**********************************************************
 *
 *   char* GetPirSeqRaw(bptr, eptr):
 *
 *                                              11-12-93
 *
 **********************************************************/
static char* GetPirSeqRaw(char* bptr, char* eptr)
{
    char* ptr;
    char* retstr = NULL;
    Int4    i;

    ptr = SrchTheChar(bptr, eptr, '\n');        /* sequence line */
    bptr = SrchTheChar(ptr, eptr, '\n');        /* head number */

    if(bptr >= eptr)
        return(NULL);

    while(bptr < eptr &&
          (isdigit(*bptr) != 0 || *bptr == ' ' || *bptr == '\n'))
        bptr++;
    
    size_t size = eptr - bptr;
    retstr = (char*) MemNew(size + 1);
    i = 0;

    while(bptr < eptr)
    {
        retstr[i] = *bptr;
        i++;
        bptr++;

        while(bptr < eptr &&
              (isdigit(*bptr) != 0 || *bptr == ' ' || *bptr == '\n'))
            bptr++;
    }

    return(retstr);
}

/**********************************************************
 *
 *   static PirBlockPtr get_pirblock(ind, sub_ind):
 *
 *                                              11-11-93
 *
 **********************************************************/
static CRef<objects::CPIR_block> get_pirblock(DataBlkPtr* ind,
                                                          DataBlkPtr** sub_ind)
{
    DataBlkPtr  dbp;
    char*     ptr;
    char*     str;

    CRef<objects::CPIR_block> pir(new objects::CPIR_block);

    dbp = sub_ind[ParFlatPIR_ORGANISM][ORGANISM_note];
    if(dbp != NULL)
    {
        ptr = dbp->mOffset;
        if(StringNCmp(ptr, "host", 4) == 0)
            ptr += 4;

        char* p = tata_save(ptr);
        pir->SetHost(p);
        MemFree(p);
    }

    pir->SetSummary(join_subind(ind[ParFlatPIR_SUMMARY], sub_ind[ParFlatPIR_SUMMARY]));

    if(ind[ParFlatPIR_GENETICS] != NULL)
        pir->SetGenetic(join_subind(ind[ParFlatPIR_GENETICS], sub_ind[ParFlatPIR_GENETICS]));

    if (ind[ParFlatPIR_CONTAINS] != NULL)
    {
        char* p = tata_save(ind[ParFlatPIR_CONTAINS]->mOffset);
        pir->SetIncludes(p);
        MemFree(p);
    }

    if(ind[ParFlatPIR_CLASSIFICATION] != NULL)
    {
        dbp = sub_ind[ParFlatPIR_CLASSIFICATION][CLASS_superfamily];
        if (dbp != NULL)
        {
            char* p = tata_save(dbp->mOffset);
            pir->SetSuperfamily(p);
            MemFree(p);
        }
    }

    pir->SetDate(join_subind(ind[ParFlatPIR_DATE], sub_ind[ParFlatPIR_DATE]));

    if(ind[ParFlatPIR_KEYWORDS] != NULL)
    {
        ptr = tata_save(ind[ParFlatPIR_KEYWORDS]->mOffset);

        TKeywordList kwds;
        split_str(kwds, ptr);
        MemFree(ptr);

        if (!kwds.empty())
            pir->SetKeywords().swap(kwds);
    }

    ptr = ind[ParFlatPIR_SEQUENCE]->mOffset;
    str = GetPirSeqRaw(ptr, ptr + ind[ParFlatPIR_SEQUENCE]->len);

    for(ptr = str; *ptr != '\0'; ptr++)
        if(isalpha(*ptr) == 0)
            break;

    if(*ptr != '\0')
    {
        pir->SetHad_punct(true);
        pir->SetSeq_raw(str);
    }
    else
        MemFree(str);

    return pir;
}

/**********************************************************
 *
 *   CRef<objects::CDate_std> GetStdDate(ptr, flag):
 *
 *      Return Date-std pointer.
 *      *flag = FALSE if there is a bad date format.
 *      ptr has format: dd-mmm-yyyy or dd-mmm-yy.
 *
 *                                              8-12-93
 *
 **********************************************************/
static CRef<objects::CDate_std> GetStdDate(char* ptr, bool* flag)
{
    Char    buf[10];

    CRef<objects::CDate_std> date(new objects::CDate_std);
    
    StringNCpy(buf, ptr, 2);
    buf[2] = '\0';
    date->SetDay(atoi(buf));

    ptr += 3;

    Int2 month = StringMatchIcase(month_name, ptr);
    date->SetMonth(month);

    if(month > 0)
        *flag = true;
    else
        *flag = false;

    ptr += 4;
    if(isdigit(ptr[2]) != 0)
    {
        StringNCpy(buf, ptr, 4);
        buf[4] = '\0';

        date->SetYear(atoi(buf));
    }
    else
    {
        CTime time(CTime::eCurrent);
        objects::CDate_std now(time);

        int cur_year = now.GetYear();

        StringNCpy(buf, ptr, 2);
        buf[2] = '\0';

        int year = atoi(buf);
        if (year + 1900 > cur_year)
            year += 1900;
        else
            year += 2000;
        date->SetYear(year);
    }

    return date;
}

/**********************************************************/
static bool CkStdMonth(const objects::CDate_std& date)
{
    Uint1        day;
    Uint1        month;
    Uint1        year;
    Uint1        last;
    static Uint1 days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    year = date.GetYear();
    month = date.GetMonth();
    day = date.GetDay();
    last = days[month-1];
    if(month == 2 && year % 4 == 0)
        last = 29;
    if(day > last || day == 0)
        return false;
    return true;
}

/**********************************************************
 *
 *   static void parse_pir_date(sub_ind, descrptr,
 *                                    date):
 *
 *      create-date is the date followed by the keyword
 *   "DATE", if it is missing, then take the eariest date
 *   of #sequence_revision or #text_change.
 *      update-date is the latest date of #sequence_revision
 *   or #text_change.
 *
 **********************************************************/
static void parse_pir_date(DataBlkPtr** sub_ind,
                           TSeqdescList& descrs, char* date)
{
    char*    seqdate;
    char*    textdate;
    bool       flag;

    DataBlkPtr dbp;

    Char       buf[12];

    if(date == NULL)
        return;

    CRef<objects::CDate_std> cdate,
                                         sdate,
                                         tdate;
            
    dbp = sub_ind[ParFlatPIR_DATE][DATE_sequence_revision];
    if(dbp != NULL)
    {
        seqdate = dbp->mOffset;
        sdate = GetStdDate(seqdate, &flag);
        flag &= CkStdMonth(*sdate);
        if(!flag)
        {
            StringNCpy(buf, seqdate, 11);
            buf[11] = '\0';
            ErrPostEx(SEV_WARNING, ERR_DATE_IllegalDate,
                      "Illegal create-date dropped:  %s", buf);
            sdate.Reset();
        }
    }

    dbp = sub_ind[ParFlatPIR_DATE][DATE_text_change];
    if(dbp != NULL)
    {
        textdate = dbp->mOffset;
        tdate = GetStdDate(textdate, &flag);
        flag &= CkStdMonth(*tdate);

        if(!flag)
        {
            StringNCpy(buf, textdate, 11);
            buf[11] = '\0';
            ErrPostEx(SEV_WARNING, ERR_DATE_IllegalDate,
                      "Illegal update-date dropped: %s", buf);
            tdate.Reset();
            sdate.Reset();
        }
    }

    dbp = sub_ind[ParFlatPIR_DATE][DATE_];
    if(dbp != NULL && *(dbp->mOffset) != '\0')
    {
        cdate = GetStdDate(dbp->mOffset, &flag);
        flag &= CkStdMonth(*cdate);

        if(!flag)
        {
            StringNCpy(buf, dbp->mOffset, 11);
            buf[11] = '\0';
            ErrPostEx(SEV_WARNING, ERR_DATE_IllegalDate,
                      "Illegal date: %s", buf);
            
            sdate.Reset(); // TODO: check why there is sdate, not cdate
        }
    }
    else                                /* missing create-date */
    {
        if (sdate.NotEmpty() || tdate.NotEmpty())
        {
            if (sdate.Empty())
                cdate = tdate;
            else if (tdate.Empty())
                cdate = sdate;
            else
            // both dates are present
            {
                if (sdate->Compare(*tdate) == objects::CDate::eCompare_after)
                    cdate = sdate;
                else
                    cdate = tdate;
            }
        }
    }


    CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
    descr->SetCreate_date().SetStd(*cdate);
    descrs.push_back(descr);

    if (sdate.NotEmpty() || tdate.NotEmpty())
    {
        descr.Reset(new objects::CSeqdesc);
        

        if (sdate.Empty())
            descr->SetUpdate_date().SetStd(*tdate);
        else if (tdate.Empty())
            descr->SetUpdate_date().SetStd(*sdate);
        else
        // both dates are present
        {
            if (sdate->Compare(*tdate) == objects::CDate::eCompare_after)
                descr->SetUpdate_date().SetStd(*sdate);
            else
                descr->SetUpdate_date().SetStd(*tdate);
        }

        descrs.push_back(descr);
    }
}

/**********************************************************/
static void DelQuotBtwData(char* value)
{
    char* p;
    char* q;

    for(p = value, q = p; *p != '\0'; p++)
        if(*p != '"')
            *q++ = *p;
    *q = '\0';
}

/**********************************************************
 *
 *   char* ReplaceNewlineToBlank(bptr, eptr):
 *
 *      Return a string without newline characters and
 *   front blanks after newline character found.
 *
 *                                              6-28-93
 *
 **********************************************************/
static char* ReplaceNewlineToBlank(char* bptr, char* eptr)
{
    char* p;
    char* q;
    char* line;
    Char    ch;

    if(bptr == NULL || eptr == NULL || bptr >= eptr)
        return(NULL);

    ch = *eptr;
    *eptr = '\0';
    line = StringSave(bptr);
    *eptr = ch;

    for(p = line; *p != '\0'; p++)
        if(*p == '\t' || *p == '\n')
            *p = ' ';

    for(p = line, q = line; *p != '\0';)
    {
        *q++ = *p;
        if(*p == ' ')
            while(*p == ' ')
                p++;
        else
            p++;
    }
    if(q > line && *(q - 1) == ' ')
        q--;
    *q = '\0';

    if(*line == ' ')
        fta_StringCpy(line, line + 1);

    return(line);
}

/**********************************************************/
static CRef<objects::COrg_ref> parse_pir_org_ref(DataBlkPtr** sub_ind,
                                                             ParserPtr pp)
{
    std::string taxstr,
                comstr;

    char*    ptr1;
    char*    ptr2;

    DataBlkPtr dbp;
    char*    p;
    Uint1      drop;

    objects::COrg_ref::TSyn syns;

    dbp = sub_ind[ParFlatPIR_ORGANISM][ORGANISM_formal_name];
    if(dbp != NULL)
    {
        p = ReplaceNewlineToBlank(dbp->mOffset, dbp->mOffset + dbp->len);
        ptr1 = p;
        DelQuotBtwData(ptr1);
        ptr2 = StringChr(ptr1, ',');

        std::string text;
        if(ptr2 != NULL)
        {
            text.assign(ptr1, ptr2);
            while(*ptr2 == ' ' || *ptr2 == ',')
                ptr2++;

            ptr1 = ptr2;

            while((ptr2 = StringChr(ptr1, ',')) != NULL)
            {
                syns.push_back(std::string(ptr1, ptr2));

                while(*ptr2 == ' ' || *ptr2 == ',')
                    ptr2++;

                ptr1 = ptr2;
            }

            if(*ptr1 != '\0')
                syns.push_back(ptr1);
        }
        else
        {
            text = NStr::Sanitize(ptr1);
        }
        taxstr = NStr::Sanitize(strip_organelle(text.c_str()));
        if(p != NULL)
        {
            MemFree(p);
        }
    }
    dbp = sub_ind[ParFlatPIR_ORGANISM][ORGANISM_common_name];
    if(dbp != NULL)
    {
        p = ReplaceNewlineToBlank(dbp->mOffset, dbp->mOffset + dbp->len);
        ptr1 = p;

        ptr2 = StringChr(ptr1, ',');
        if(ptr2 != NULL)
        {
            comstr.assign(ptr1, ptr2);

            while(*ptr2 == ' ' || *ptr2 == ',')
                ptr2++;

            ptr1 = ptr2;

            while((ptr2 = StringChr(ptr1, ',')) != NULL)
            {
                syns.push_back(std::string(ptr1, ptr2));

                while(*ptr2 == ' ' || *ptr2 == ',')
                    ptr2++;

                ptr1 = ptr2;
            }
            if(*ptr1 != '\0')
                syns.push_back(ptr1);
        }
        else
        {
            comstr = NStr::Sanitize(ptr1);
        }
        if(p != NULL)
        {
            MemFree(p);
        }
    }

    if(taxstr.empty())
    {
        if(comstr.empty())
        {
            ErrPostEx(SEV_WARNING, ERR_ORGANISM_NoFormalName,
                      "Both formal and common names are not found in this entry");
        }
        else
        {
            ErrPostEx(SEV_WARNING, ERR_ORGANISM_NoFormalName,
                      "No formal name found for this entry");
        }
    }

    CRef<objects::COrg_ref> org_ref;
    if (taxstr.empty() && comstr.empty() && syns.empty())
    {
        ErrPostEx(SEV_ERROR, ERR_ORGANISM_NoOrganism,
                  "Could not get organism from input data.");
        return org_ref;
    }

    org_ref.Reset(new objects::COrg_ref);

    if(taxstr.empty())
        org_ref->SetTaxname(comstr);
    else
    {
        org_ref->SetTaxname(taxstr);
        org_ref->SetCommon(comstr);
    }

    org_ref->SetSyn().swap(syns);
    drop = 0;

    fta_fix_orgref(pp, *org_ref, &drop, NULL);
    return org_ref;
}

/**********************************************************
 *
 *   static DatePtr get_pir_date(s):
 *
 *      Get year and month, and return NCBI_DatePtr
 *   for PIR submissions.
 *
 *                                              12-4-93
 *
 **********************************************************/
static CRef<objects::CDate> get_pir_date(char* s)
{
    static const char *months[12] = {"January", "February", "March",
                                     "April",   "May",      "June",
                                     "July",    "August",   "September",
                                     "October", "November", "December"};
    const char        *p;
    int               month = 0;
    int               year;
    int               i;

    for(p = s; *p != ' ';)
        p++;
    year = atoi(p + 1);

    for(i = 0; i < 12; i++)
    {
        if(StringNCmp(s, months[i], StringLen(months[i])) == 0)
        {
            month = i + 1;
            break;
        }
    }

    CRef<objects::CDate> date(new objects::CDate);
    CTime time(CTime::eCurrent);
    date->SetStd().SetToTime(time);

    if(year < 1900 || year > date->GetStd().GetYear() + 1)
    {
        ErrPostEx(SEV_WARNING, ERR_REFERENCE_IllegalDate,
                  "Illegal year: %d", year);
    }

    date->Reset();
    date->SetStd().SetYear(year);
    date->SetStd().SetMonth(month);

    i = XDateCheck(date->GetStd());

    if(i < -1 && i > -5)
        p = "month not set";
    else if(i == -1)
        p = "day not set";
    else if(i == 1)
        p = "invalid day";
    else if(i == 2)
        p = "invalid month";
    else if(i == 3)
        p = "year not set";
    else
        p = NULL;
    if(p != NULL)
        ErrPostEx(SEV_WARNING, ERR_REFERENCE_DateCheck,
                  "Illegal date, %s: %s", p, s);
    if (i < 1)
        return date;

    date.Reset();
    return date;
}

/**********************************************************/
static CRef<objects::CCit_art> get_pir_book(char* bptr, CRef<objects::CAuth_list>& auth_list, CRef<objects::CTitle::C_E>& title)
{
    char*    s;
    const Char* str;
    char*    pages;
    char*    ed;
    char*    au;
    char*    eptr;

    if(StringNCmp(bptr, "in ", 3) == 0)
        bptr += 3;

    for(au = bptr; *au != ',';)
        au++;

    CRef<objects::CTitle::C_E> book_title(new objects::CTitle::C_E);
    book_title->SetName(std::string(bptr, au));

    eptr = bptr + StringLen(bptr) - 1;

    CRef<objects::CCit_art> cit_art(new objects::CCit_art);
    objects::CCit_book& cit_book = cit_art->SetFrom().SetBook();

    if((ed = StringStr(bptr, "ed.,")) != NULL ||
       (ed = StringStr(bptr, "eds.,")) != NULL)
    {
        for(s = ed; *s != ',' && s >= au;)
            s--;
        *s-- = '\0';
        if(s[1] == ' ')
        {
            fta_StringCpy(s + 1, s + 2);
        }
        CRef<objects::CAuth_list> book_auth_list;
        get_auth(au, PIR_REF, NULL, book_auth_list);
        if (book_auth_list.NotEmpty())
            cit_book.SetAuthors(*book_auth_list);
    }

    if (!cit_book.IsSetAuthors())
    {
        ErrPostEx(SEV_WARNING, ERR_REFERENCE_MissingBookAuthors,
                  "Missing book author, %s", bptr);

        cit_art.Reset();
        return cit_art;
    }

    cit_book.SetTitle().Set().push_back(book_title);

    s = StringStr(ed, "p.");
    if(s == NULL)
    {
        ErrPostEx(SEV_WARNING, ERR_REFERENCE_MissingBookPages,
                  "Wrong format: missing 'pp.' for pages, %s", bptr);
        for(pages = ed; *pages != ',' && *pages != '\0';)
            pages++;
        while(isalnum(*pages) == 0)
            pages++;
        if(isdigit(*pages) != 0)
        {
            for(s = pages; *s != ',' && *s != '\0';)
                s++;
        }
        else
        {
            ErrPostEx(SEV_WARNING, ERR_REFERENCE_MissingBookPages,
                      "Missing pages, %s", bptr);
            s = pages - 1;
            pages = NULL;
        }
    }
    else
    {
        for(s += 2; isspace(*s) != 0;)
            s++;
        for(pages = s; *s != ',' && *s != '\0';)
            s++;
    }
    if(*s == '\0')
    {
        ErrPostStr(SEV_WARNING, ERR_REFERENCE_Book,
                   "Missing publisher for the book");
    }

    *s++ = '\0';
    str = s;

    objects::CImprint& imp = cit_book.SetImp();
    if(pages != NULL)
    {
        str = (book_title.Empty()) ? " " : book_title->GetName().c_str();

        if(valid_pages_range(pages, str, 0, false) > 0)
            imp.SetPages(NStr::Sanitize(pages));
    }

    for(s = eptr; isdigit((int) *s) != 0;)
        s--;
    if(s == eptr)
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                  "Fail to parse book: missing date");
        cit_art.Reset();
        return cit_art;
    }

    CRef<objects::CDate> date = get_date(s + 1);
    imp.SetDate(*date);

    *s = '\0';

    imp.SetPub().SetStr(NStr::Sanitize(str));

    if (auth_list.NotEmpty() && auth_list->IsSetNames())
        cit_art->SetAuthors(*auth_list);

    if (title.NotEmpty())
        cit_art->SetTitle().Set().push_back(title);

    return(cit_art);
}

/**********************************************************/
static CRef<objects::CCit_sub> get_pir_sub(char* bptr, CRef<objects::CAuth_list>& auth_list)
{
    CRef<objects::CCit_sub> cit_sub;
    char*   s;
    char*   eptr;

    if(bptr == NULL)
        return cit_sub;

    if (auth_list.Empty() || !auth_list->IsSetNames())
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                  "Fail to parse direct submission: missing author");
        return cit_sub;
    }

    CRef<objects::CDate> date;
    eptr = bptr + StringLen(bptr) - 1;
    for(s = bptr; *s != ',' && *s != '\0';)
        s++;
    if(*s == ',')
    {
        eptr = s;
        for(s++; *s == ' ';)
            s++;
        date = get_pir_date(s);
    }

    if(date.Empty())
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                  "Fail to parse direct submission: illegal date");
        return cit_sub;
    }

    cit_sub.Reset(new objects::CCit_sub);

    cit_sub->SetDate(*date);
    if (!auth_list->IsSetAffil())
    {
        if(StringNCmp(bptr, "submitted", 9) == 0)
            for(bptr += 9; *bptr == ' ';)
                bptr++;

        auth_list->SetAffil().SetStr(std::string(bptr, eptr));
    }

    cit_sub->SetAuthors(*auth_list);
    cit_sub->SetMedium(objects::CCit_sub::eMedium_other);

    return(cit_sub);
}

/**********************************************************/
static CRef<objects::CCit_gen> get_pir_cit(char* bptr, CRef<objects::CAuth_list>& auth_list, CRef<objects::CTitle::C_E>& title, int muid)
{
    CRef<objects::CCit_gen> cit_gen(new objects::CCit_gen);

    cit_gen->SetCit(bptr);

    if (auth_list.NotEmpty() && auth_list->IsSetNames())
        cit_gen->SetAuthors(*auth_list);

    if(title.NotEmpty())
        cit_gen->SetTitle(title->GetName());

    if(muid != 0)
        cit_gen->SetMuid(ENTREZ_ID_FROM(int, muid));

    return cit_gen;
}

/**********************************************************/
static CRef<objects::CCit_gen> get_pir_sub_gen(char* bptr, CRef<objects::CAuth_list>& auth_list)
{
    CRef<objects::CCit_gen> cit_gen;

    if(bptr == NULL)
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                  "Fail to parse direct submission: empty data");
        return cit_gen;
    }

    if (auth_list.Empty() || !auth_list->IsSetNames())
    {
        ErrPostEx(SEV_ERROR, ERR_REFERENCE_Fail_to_parse,
                  "Fail to parse direct submission: missing author");
        return cit_gen;
    }

    cit_gen.Reset(new objects::CCit_gen);
    cit_gen->SetCit(NStr::Sanitize(bptr));
    cit_gen->SetAuthors(*auth_list);

    return cit_gen;
}

/**********************************************************/
static CRef<objects::CCit_art> pir_journal(char* bptr, CRef<objects::CAuth_list>& auth_list, CRef<objects::CTitle::C_E>& title)
{
    char*    s;
    char*    tit;
    char*    volume;
    char*    pages;

    CRef<objects::CCit_art> cit_art;

    s = bptr;
    for(tit = s; isdigit((int) *s) == 0 && *s != '\0';)
        s++;

    CRef<objects::CDate> date = get_date(s);

    if(*s == '\0' || date.Empty())
    {
        ErrPostEx(SEV_WARNING, ERR_REFERENCE_Illegalreference,
                  "Journal format error: %s", bptr);
        return cit_art;
    }

    *(s - 1) = '\0';

    /* Create Asn.1 structure for an article
     */
    cit_art.Reset(new objects::CCit_art);

    if (title.NotEmpty())
        cit_art->SetTitle().Set().push_back(title);

    /* Create Asn.1 structure for a journal
     */
    objects::CCit_jour& journal = cit_art->SetFrom().SetJournal();

    if (tit)
    {
        CRef<objects::CTitle::C_E> journal_title(new objects::CTitle::C_E);
        journal_title->SetIso_jta(NStr::Sanitize(tit));
        journal.SetTitle().Set().push_back(journal_title);
    }

    /* Imprint
     */
    objects::CImprint& imp = journal.SetImp();
    imp.SetDate(*date);

    for(s++; *s != ')' && *s != '\0';)
        s++;
    for(s++; *s == ' ';)
        s++;
    for(volume = s; *s != ':' && *s != '\0';)
        s++;
    if(*s != '\0')
    {
        *s++ = '\0';
        pages = s;
        imp.SetVolume(volume);
        imp.SetPages(pages);
    }
    else
        imp.SetPrepub(objects::CImprint::ePrepub_in_press);

    if (auth_list.NotEmpty())
        cit_art->SetAuthors(*auth_list);
    return(cit_art);
}

/**********************************************************/
static CRef<objects::CPubdesc> get_pir_ref(DataBlkPtr* sub_ind)
{
    DataBlkPtr  dbp;

    Int4        muid = 0;
    Int4        pmid = 0;

    char*     bptr;
    char*     p;
    bool        badart;

    CRef<objects::CPubdesc> desc;
    if(((sub_ind[REFERENCE_journal] != NULL) +
        (sub_ind[REFERENCE_citation] != NULL) +
        (sub_ind[REFERENCE_book] != NULL) +
        (sub_ind[REFERENCE_submission] != NULL)) > 1)
    {
        ErrPostStr(SEV_ERROR, ERR_REFERENCE_Illegalreference,
                   "Can't define asn.1 pub type (redeclaration)!");
        return desc;
    }

    desc.Reset(new objects::CPubdesc);

    CRef<objects::CAuth_list> auth_list;
    dbp = sub_ind[REFERENCE_authors];
    if(dbp != NULL)
    {
        bptr = ReplaceNewlineToBlank(dbp->mOffset, dbp->mOffset + dbp->len);

        if (StringNICmp(bptr, "anonymous, ", 11) == 0)
            get_auth_consortium(bptr + 11, auth_list);
        else
            get_auth(bptr, PIR_REF, NULL, auth_list);

        if(bptr != NULL)
            MemFree(bptr);
    }

    CRef<objects::CTitle::C_E> title_art;
    dbp = sub_ind[REFERENCE_title];
    if(dbp != NULL)
    {
        bptr = ReplaceNewlineToBlank(dbp->mOffset, dbp->mOffset + dbp->len);
        if (bptr != NULL)
        {
            if (bptr[0])
            {
                title_art.Reset(new objects::CTitle::C_E);
                title_art->SetName(NStr::Sanitize(bptr));
            }
            MemFree(bptr);
        }
    }

    CRef<objects::CPub> pub_ref(new objects::CPub);

    badart = false;
    dbp = sub_ind[REFERENCE_journal];
    if(dbp != NULL)
    {
        bptr = ReplaceNewlineToBlank(dbp->mOffset, dbp->mOffset + dbp->len);

        CRef<objects::CCit_art> cit_art = pir_journal(bptr, auth_list, title_art);

        if (cit_art.Empty())
        {
            pub_ref->SetGen(*get_error(bptr, auth_list, title_art));
            badart = true;
        }
        else
            pub_ref->SetArticle(*cit_art);

        if(bptr != NULL)
            MemFree(bptr);
    }
    else if((dbp = sub_ind[REFERENCE_citation]) != NULL)
    {
        bptr = ReplaceNewlineToBlank(dbp->mOffset, dbp->mOffset + dbp->len);
        CRef<objects::CCit_gen> cit_gen = get_pir_cit(bptr, auth_list, title_art, muid);

        if (cit_gen.NotEmpty())
            pub_ref->SetGen(*cit_gen);

        if(bptr != NULL)
            MemFree(bptr);
    }
    else if((dbp = sub_ind[REFERENCE_book]) != NULL)
    {
        bptr = ReplaceNewlineToBlank(dbp->mOffset, dbp->mOffset + dbp->len);

        CRef<objects::CCit_art> cit_art = get_pir_book(bptr, auth_list, title_art);

        if (cit_art.Empty())
        {
            pub_ref->SetGen(*get_error(bptr, auth_list, title_art));
            badart = true;
        }
        else
            pub_ref->SetArticle(*cit_art);

        if(bptr != NULL)
            MemFree(bptr);
    }
    else if((dbp = sub_ind[REFERENCE_submission]) != NULL)
    {
        bptr = ReplaceNewlineToBlank(dbp->mOffset, dbp->mOffset + dbp->len);

        CRef<objects::CCit_sub> cit_sub = get_pir_sub(bptr, auth_list);

        if (cit_sub.Empty())
            pub_ref->SetGen(*get_pir_sub_gen(bptr, auth_list));
        else
            pub_ref->SetSub(*cit_sub);

        if(bptr != NULL)
            MemFree(bptr);
    }

    dbp = sub_ind[REFERENCE_description];
    if(dbp != NULL)
    {
        bptr = ReplaceNewlineToBlank(dbp->mOffset, dbp->mOffset + dbp->len);
        if(bptr != NULL)
            MemFree(bptr);
    }

    dbp = sub_ind[REFERENCE_contents];
    if(dbp != NULL)
    {
        bptr = ReplaceNewlineToBlank(dbp->mOffset, dbp->mOffset + dbp->len);

        desc->SetComment(NStr::Sanitize(bptr));

        if(bptr != NULL)
            MemFree(bptr);
    }

    dbp = sub_ind[REFERENCE_note];
    if(dbp != NULL)
    {
        bptr = ReplaceNewlineToBlank(dbp->mOffset, dbp->mOffset + dbp->len);

        if (desc->IsSetComment())
            desc->SetComment() += ";~";
        desc->SetComment() += NStr::Sanitize(bptr);

        if(bptr != NULL)
            MemFree(bptr);
    }

    dbp = sub_ind[REFERENCE_cross_references];
    if(dbp != NULL)
    {
        bptr = ReplaceNewlineToBlank(dbp->mOffset, dbp->mOffset + dbp->len);
        p = StringStr(bptr, "MUID:");
        if(p != NULL)
            muid = atoi(p + 5);
        p = StringStr(bptr, "PMID:");
        if(p != NULL)
            pmid = atoi(p + 5);
        if(badart)
        {
            if(muid > 0)
                ErrPostEx(SEV_WARNING, ERR_REFERENCE_MuidIgnored,
                          "Because reference data could not be parsed correctly, supplied Medline UI %d is being ignored.",
                          muid);
            if(pmid > 0)
                ErrPostEx(SEV_WARNING, ERR_REFERENCE_PmidIgnored,
                          "Because reference data could not be parsed correctly, supplied PubMed ID %d is being ignored.",
                          pmid);
        }
        else
        {
            if(muid > 0 && pmid > 0 && muid == pmid)
                muid = 0;

            if(muid > 0)
            {
                CRef<objects::CPub> pub(new objects::CPub);
                pub->SetMuid(ENTREZ_ID_FROM(int, muid));
                desc->SetPub().Set().push_back(pub);
            }

            if(pmid > 0)
            {
                CRef<objects::CPub> pub(new objects::CPub);
                pub->SetPmid().Set(ENTREZ_ID_FROM(int, pmid));
                desc->SetPub().Set().push_back(pub);
            }
        }
        if(bptr != NULL)
            MemFree(bptr);
    }

    desc->SetPub().Set().push_back(pub_ref);
    return desc;
}

/**********************************************************
 *
 *   static ValNodePtr get_pir_descr(ind, sub_ind):
 *
 *                                              11-11-93
 *
 **********************************************************/
static void get_pir_descr(DataBlkPtr* ind,
                          DataBlkPtr** sub_ind,
                          ParserPtr pp,
                          TSeqdescList& descrs)
{
    DataBlkPtr   dbp;

    char*      offset;
    char*      str;

    CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
    descr->SetTitle("");

    if(ind[ParFlatPIR_TITLE] != NULL)
    {
        str = ind[ParFlatPIR_TITLE]->mOffset;
        char* p = tata_save(str);
        descr->SetTitle(p);
        MemFree(p);
    }
    descrs.push_back(descr);

    for(dbp = ind[ParFlatPIR_COMMENT]; dbp != NULL; dbp = dbp->mpNext)
    {
        offset = dbp->mOffset;
        char* p = tata_save(offset);

        if (p && p[0])
        {
            descr.Reset(new objects::CSeqdesc);
            descr->SetComment(p);
            descrs.push_back(descr);
        }
        MemFree(p);
    }

    for(dbp = ind[ParFlatPIR_COMPLEX]; dbp != NULL; dbp = dbp->mpNext)
    {
        offset = dbp->mOffset;
        char* p = tata_save(offset);

        if (p && p[0])
        {
            descr.Reset(new objects::CSeqdesc);
            descr->SetComment(p);
            descrs.push_back(descr);
        }
        MemFree(p);
    }

    /* pir-block
     */
    CRef<objects::CPIR_block> pir = get_pirblock(ind, sub_ind);

    descr.Reset(new objects::CSeqdesc);
    descr->SetPir(*pir);
    descrs.push_back(descr);


    if(ind[ParFlatPIR_DATE] != NULL)
    {
        offset = ind[ParFlatPIR_DATE]->mOffset;
        parse_pir_date(sub_ind, descrs, offset);
    }

    if(ind[ParFlatPIR_ORGANISM] != NULL)
    {
        offset = ind[ParFlatPIR_ORGANISM]->mOffset;
        CRef<objects::COrg_ref> org_ref = parse_pir_org_ref(sub_ind, pp);
        if (org_ref.NotEmpty())
        {
            CRef<objects::CBioSource> bio_src(new objects::CBioSource);
            bio_src->SetOrg(*org_ref);

            int genome = GetPirGenome(sub_ind);
            if (genome != objects::CBioSource::eGenome_unknown)
                bio_src->SetGenome(genome);

            descr.Reset(new objects::CSeqdesc);
            descr->SetSource(*bio_src);
            descrs.push_back(descr);
        }
    }
    else
    {
        ErrPostEx(SEV_WARNING, ERR_ORGANISM_NoOrganism,
                  "No ORGANISM field found in this entry");
        if (!pp->debug)
        {
            descrs.clear();
            return;
        }
    }

    /* GIBB_mol will go to MolInfo
     */
    CRef<objects::CMolInfo> mol_info(new objects::CMolInfo);
    mol_info->SetBiomol(objects::CMolInfo::eBiomol_peptide);

    descr.Reset(new objects::CSeqdesc);
    descr->SetMolinfo(*mol_info);
    descrs.push_back(descr);

    if(ind[ParFlatPIR_REFERENCE] != NULL)
    {
        for(dbp = ind[ParFlatPIR_REFERENCE]; dbp != NULL; dbp = dbp->mpNext)
        {
            CRef<objects::CPubdesc> pubdesc = get_pir_ref((DataBlkPtr*) dbp->mpData);
            if (pubdesc.NotEmpty())
            {
                CRef<objects::CSeqdesc> descr(new objects::CSeqdesc);
                descr->SetPub(*pubdesc);
                descrs.push_back(descr);
            }
        }
    }
}

/**********************************************************
 *
 *   static SeqFeatPtr pir_prot_ref(ind, length):
 *
 *      Modified from PirFeatProtRef().
 *
 **********************************************************/
static CRef<objects::CSeq_feat> pir_prot_ref(DataBlkPtr* ind, size_t length)
{
    char*    offset;
    char*    bptr;
    char*    eptr;
    char*    str;
    char*    str1;

    CRef<objects::CSeq_feat> feat;
    
    if (ind[ParFlatPIR_TITLE] == NULL)
        return feat;

    offset = ind[ParFlatPIR_TITLE]->mOffset;

    CRef<objects::CProt_ref> prot_ref(new objects::CProt_ref);

    str1 = StringStr(offset, "(EC");
    if(str1 != NULL)
    {
        bptr = str1 + 3;
        *(str1 - 1) = '\0';
        for(eptr = bptr; *eptr != '\0' && *eptr != ')';)
            eptr++;
        *eptr = '\0';

        char* p = tata_save(bptr);
        prot_ref->SetEc().push_back(p);
        MemFree(p);

        str = StringCat(offset, eptr + 1);
    }
    else
        str = offset;

    bptr = StringStr(str, " - ");
    if(bptr != NULL)
        *bptr = '\0';

    char* p = tata_save(str);
    prot_ref->SetName().push_back(p);
    MemFree(p);

    if(ind[ParFlatPIR_ALTERNATE_NAMES] != NULL)
    {
        offset = ind[ParFlatPIR_ALTERNATE_NAMES]->mOffset;
        str = ReplaceNewlineToBlank(offset, offset +
                                    ind[ParFlatPIR_ALTERNATE_NAMES]->len);

        objects::CProt_ref::TName names;
        split_str(names, str);
        MemFree(str);

        prot_ref->SetName().splice(prot_ref->SetName().end(), names);
    }

    if(ind[ParFlatPIR_ENTRY] != NULL)
        bptr = ind[ParFlatPIR_ENTRY]->mOffset;

    feat.Reset(new objects::CSeq_feat);
    feat->SetData().SetProt(*prot_ref);

    CRef<objects::CSeq_id> id(MakeLocusSeqId(bptr, objects::CSeq_id::e_Pir));
    CRef<objects::CSeq_loc> locs = fta_get_seqloc_int_whole(*id, length);

    if (locs.NotEmpty())
        feat->SetLocation(*locs);

    return feat;
}


/**********************************************************
 *
 *   char* PirStringCombine(str1, str2):
 *
 *      Return a string which is combined str1 and str2,
 *   put blank between two strings; also memory free out
 *   str1 and str2.
 *
 *                                              10-18-93
 *
 **********************************************************/
static char* PirStringCombine(char* str1, char* str2)
{
    char* newstr;

    if(str1 == NULL)
        return(str2);
    if(str2 == NULL)
        return(str1);

    newstr = (char*) MemNew(StringLen(str1) + StringLen(str2) + 2);
    StringCpy(newstr, str1);
    StringCat(newstr, " ");
    StringCat(newstr, str2);
    MemFree(str1);
    MemFree(str2);
    return(newstr);
}

/**********************************************************
 *
 *   static SeqLocPtr GetPirSeqLocInt(str, seqid):
 *
 *                                              11-16-93
 *
 **********************************************************/
static CRef<objects::CSeq_loc> GetPirSeqLocInt(const Char* str, objects::CSeq_id& seqid)
{
    const Char* ptr;
    Int4      from;
    Int4      to;

    CRef<objects::CSeq_loc> loc;
    if(str == NULL)
        return loc;

    for(ptr = str; *ptr != '\0' && isdigit(*ptr) != 0;)
        ptr++;

    if(ptr == str)
        return loc;

    from = NStr::StringToInt(str, NStr::fAllowTrailingSymbols);

    while(*ptr != '\0' && isdigit(*ptr) == 0)
        ptr++;

    to = NStr::StringToInt(ptr, NStr::fAllowTrailingSymbols);


    loc.Reset(new objects::CSeq_loc);
    objects::CSeq_interval& interval = loc->SetInt();

    interval.SetFrom((from > 0) ? (from - 1) : 0);
    interval.SetTo((to > 0) ? (to - 1) : 0);
    interval.SetId(seqid);

    return loc;
}

/**********************************************************
 *
 *   static SeqPntPtr GetPirSeqLocPntPtr(str, seqid):
 *
 *                                              11-16-93
 *
 **********************************************************/
static CRef<objects::CSeq_point> GetPirSeqLocPntPtr(const Char* str, objects::CSeq_id& seqid)
{
    CRef<objects::CSeq_point> ret;

    Int4      point;

    if(str == NULL)
        return ret;

    ret.Reset(new objects::CSeq_point);
    point = NStr::StringToInt(str, NStr::fAllowTrailingSymbols);

    ret->SetPoint((point > 0) ? (point - 1) : 0);
    ret->SetId(seqid);

    return ret;
}

/**********************************************************
 *
 *   static SeqLocPtr GetPirSeqLocBond(str, seqid):
 *
 *                                              11-16-93
 *
 **********************************************************/
static CRef<objects::CSeq_loc> GetPirSeqLocBond(const Char* str, objects::CSeq_id& seqid)
{
    CRef<objects::CSeq_loc> loc;

    const Char* ptr;

    if(str == NULL)
        return loc;

    loc.Reset(new objects::CSeq_loc);
    objects::CSeq_bond& bond = loc->SetBond();

    if(StringChr(str, '-') != NULL)
    {
        for(ptr = str; *ptr != '\0' && isdigit(*ptr) != 0;)
            ptr++;

        bond.SetA(*GetPirSeqLocPntPtr(str, seqid));

        while(*ptr != '\0' && isdigit(*ptr) == 0)
            ptr++;
        if(*ptr != '\0')
            bond.SetB(*GetPirSeqLocPntPtr(ptr, seqid));
    }
    else
        bond.SetA(*GetPirSeqLocPntPtr(str, seqid));

    return loc;
}

/**********************************************************
 *
 *   static SeqLocPtr GetPirSeqLocPnt(str, seqid):
 *
 *                                              11-16-93
 *
 **********************************************************/
static CRef<objects::CSeq_loc> GetPirSeqLocPnt(const Char* str, objects::CSeq_id& seqid)
{
    CRef<objects::CSeq_loc> ret;
    if(str == NULL)
        return ret;

    ret.Reset(new objects::CSeq_loc);
    ret->SetPnt(*GetPirSeqLocPntPtr(str, seqid));

    return ret;
}

/**********************************************************
 *
 *   SeqLocPtr getPirSeqLocation(location, seqid, bond):
 *
 *      Algorithm to parse location:
 *      - if there is existed any commas, then SEQLOC_EQUIV
 *      - if not a SeqFeat_bond feature, then
 *        - an interger, then SEQLOC_PNT
 *        - two integers separated by a dash, then SELOCINT
 *      - if a SeqFeat_bond feature, then
 *        - an interger, then SEQLOC_BOND, a
 *        - two intergers separated by a dash, then SELOC_BOND, a and b
 *      - it never has partial or fuzziness.
 *
 *                                              11-16-93
 *
 **********************************************************/
static CRef<objects::CSeq_loc> getPirSeqLocation(char* location, objects::CSeq_id& seqid,
                                                             bool bond)
{
    Int2      num;
    char*   ptr;
    char*   eptr;

    CRef<objects::CSeq_loc> ret;

    if (location == NULL)
        return ret;

    objects::CSeq_loc::TEquiv::Tdata locs;
    
    for(ptr = location, num = 0; *ptr != '\0'; ptr = eptr)
    {
        while(*ptr == ',' || *ptr == ' ')
            ptr++;
        for(eptr = ptr; *eptr != ',' && *eptr != '\0';)
            eptr++;
        
        std::string str(ptr, eptr);

        CRef<objects::CSeq_loc> loc;
        if(bond)
        {
            loc = GetPirSeqLocBond(str.c_str(), seqid);
        }
        else if (str.find('-') != std::string::npos)
        {
            loc = GetPirSeqLocInt(str.c_str(), seqid);
        }
        else
        {
            loc = GetPirSeqLocPnt(str.c_str(), seqid);
        }

        if (loc.Empty())
            continue;

        locs.push_back(loc);
        num++;
    }

    ret.Reset(new objects::CSeq_loc);

    if(num > 1)
        ret->SetEquiv().Set().swap(locs);
    else
        ret = *locs.begin();

    return ret;
}

/**********************************************************
 *
 *   static SeqFeatPtr pir_feat(ind, hsfp):
 *
 *                                              11-15-93
 *
 **********************************************************/
static void pir_feat(DataBlkPtr* ind, objects::CSeq_annot::C_Data::TFtable& feats)
{
    DataBlkPtr dbp;
    DataBlkPtr subdbp;

    Int2       indx;
    char*    str;
    char*    ptr;
    char*    location;
    char*    comment;
    char*    ptr1;
    char*    str1;
    char*    p;

    dbp = ind[ParFlatPIR_FEATURE];
    if(dbp == NULL)
        return;

    CRef<objects::CSeq_id> seqid;
    if (ind[ParFlatPIR_ENTRY] != NULL)
    {
        seqid = MakeLocusSeqId(ind[ParFlatPIR_ENTRY]->mOffset, objects::CSeq_id::e_Pir);
    }

    for(subdbp = (DataBlkPtr) dbp->mpData; subdbp != NULL; subdbp = subdbp->mpNext)
    {
        str = StringSave(std::string(subdbp->mOffset, subdbp->mOffset + subdbp->len).c_str());
        indx = MatchArraySubString(PirFeatInput, str);
        if(indx == -1)
        {
            ErrPostEx(SEV_WARNING, ERR_FEATURE_UnknownFeatKey,
                      "Unknown feature key found, drop the feature, %s", str);
            MemFree(str);
            continue;
        }

        location = NULL;
        ptr = StringStr(str, PirFeatInput[indx]);
        for(ptr1 = str; *ptr1 != '\0' && *ptr1 == ' ';)
            ptr1++;
        if(*ptr1 != '\0')
        {
            location = ReplaceNewlineToBlank(ptr1, ptr);
        }


        CRef<objects::CSeq_feat> feat(new objects::CSeq_feat);

        CRef<objects::CSeq_loc> loc;
        switch(PirFeat[indx].choice)
        {
        case objects::CSeqFeatData::e_Region:
                feat->SetData().SetRegion(PirFeat[indx].keystr);
                loc = getPirSeqLocation(location, *seqid, false);
                break;
        case objects::CSeqFeatData::e_Site:
                feat->SetData().SetSite(static_cast<objects::CSeqFeatData::TSite>(PirFeat[indx].keyint));
                loc = getPirSeqLocation(location, *seqid, false);
                break;
        case objects::CSeqFeatData::e_Bond:
                feat->SetData().SetBond(static_cast<objects::CSeqFeatData::EBond>(PirFeat[indx].keyint));
                loc = getPirSeqLocation(location, *seqid, true);
                break;
            default:
                break;
        }

        if (loc.Empty())
        {
            p = StringChr(str, '\n');
            if(p != NULL)
                *p = '\0';
            ErrPostEx(SEV_ERROR, ERR_FEATURE_UnparsableLocation,
                      "Unparsable location \"%s\". Feature dropped: \"%s\".",
                      location, str);
            MemFree(location);
            MemFree(str);
            continue;
        }
        else
            feat->SetLocation(*loc);

        MemFree(location);

        comment = NULL;
        ptr = StringStr(str, PirFeatInput[indx]);
        ptr += PirFeat[indx].inlen;

        while(*ptr == ' ' || *ptr == '\\')
            ptr++;

        if(*ptr != '\0')
        {
            while((ptr1 = StringChr(ptr, '\n')) != NULL)
            {
                str1 = StringSave(std::string(ptr, ptr1).c_str());
                comment = PirStringCombine(comment, str1);

                for(ptr = ptr1; *ptr == ' ' || *ptr == '\n';)
                    ptr++;
            }

            if(*ptr != '\0')
            {
                str1 = StringSave(ptr);
                comment = PirStringCombine(comment, str1);
            }
        }

        if(comment != NULL)
        {
            if(comment[StringLen(comment)-1] == '\\')
                comment[StringLen(comment)-1] = '\0';

            ptr = StringStr(comment, "#status");
            if(ptr != NULL)
            {
                ptr1 = StringStr(ptr, "experimental");
                if(ptr1 != NULL)
                    feat->SetExp_ev(objects::CSeq_feat::eExp_ev_experimental);
                else if((ptr1 = StringStr(ptr, "predicted")) != NULL)
                    feat->SetExp_ev(objects::CSeq_feat::eExp_ev_not_experimental);
                *ptr = '\0';
            }

            char* p = tata_save(comment);
            if (p != NULL && p[0] != 0)
                feat->SetComment(p);
            MemFree(p);
            MemFree(comment);
        }

        feats.push_back(feat);
        MemFree(str);
    }
}

/**********************************************************
 *
 *   static void seq_feat_equiv(sfp):
 *
 *      For PIR only.
 *   Converts a SEQLOC_EQUIV to multiple SeqFeats
 *   It is used because the ',' between intervals in the
 *   parser is first converted to an equiv, but we really
 *   want the features copied, to avoid conversion
 *   to one-of() on output.
 *
 *                                              01-12-94
 *
 **********************************************************/
static void seq_feat_equiv(objects::CSeq_feat& feat, objects::CSeq_annot::C_Data::TFtable& feats)
{
    if (!feat.IsSetLocation() || !feat.GetLocation().IsEquiv())
        return;

    /* first SeqLoc in Equiv
     */

    objects::CSeq_feat feat_copy;
    feat_copy.Assign(feat);
    feat_copy.ResetLocation();

    NON_CONST_ITERATE(objects::CSeq_loc::TEquiv::Tdata, loc, feat.SetLocation().SetEquiv().Set())
    {
        CRef<objects::CSeq_feat> new_feat(new objects::CSeq_feat);
        new_feat->Assign(feat_copy);
        new_feat->SetLocation(*(*loc));
        feats.push_back(new_feat);
    }
}

/**********************************************************
 *
 *   static SeqAnnotPtr get_pir_annot(ind, length):
 *
 *      Modified from GetPirAnnot().
 *
 **********************************************************/
static CRef<objects::CSeq_annot> get_pir_annot(DataBlkPtr* ind, size_t length)
{
    CRef<objects::CSeq_annot> annot;

    objects::CSeq_annot::C_Data::TFtable feats;

    CRef<objects::CSeq_feat> feat(pir_prot_ref(ind, length));    /* TITLE & ALTERNATE-NAME line */
    if (feat.NotEmpty())
        feats.push_back(feat);

    pir_feat(ind, feats);
    for (objects::CSeq_annot::C_Data::TFtable::iterator feat = feats.begin(); feat != feats.end(); )
    {
        objects::CSeq_annot::C_Data::TFtable new_feats;
        seq_feat_equiv(*(*feat), new_feats);

        if (!new_feats.empty())
        {
            feats.splice(feat, new_feats);
            feat = feats.erase(feat);
        }
        else
            ++feat;
    }

    if (feats.empty())
        return annot;

    annot.Reset(new objects::CSeq_annot);
    annot->SetData().SetFtable().swap(feats);

    return annot;
}

/**********************************************************/
static CRef<objects::CSeq_entry> ind2asn(ParserPtr pp, DataBlkPtr* ind,
                                                     DataBlkPtr** sub_ind, unsigned char* protconv)
{
    CRef<objects::CSeq_entry> ret;
    if(!check_pir_entry(ind))
        return ret;

    CRef<objects::CSeq_entry> entry = create_entry(ind);
    objects::CBioseq& bioseq = entry->SetSeq();

    if (!get_seq_data(ind, sub_ind, bioseq, protconv, objects::CSeq_data::e_Iupacaa))
        return ret;

    TSeqdescList descrs;
    get_pir_descr(ind, sub_ind, pp, descrs);
    if (descrs.empty())
        return ret;

    bioseq.SetDescr().Set().splice(bioseq.SetDescr().Set().end(), descrs);

    if (no_date(Parser::EFormat::PIR, bioseq.GetDescr().Get()) && !pp->debug)
    {
        ErrPostStr(SEV_ERROR, ERR_DATE_IllegalDate,
                   "Illegal create or update date, entry dropped");
        return ret;
    }

    if(no_reference(bioseq))
    {
        ErrPostStr(SEV_ERROR, ERR_REFERENCE_No_references,
                   "No reference for the entry, entry dropped");
        return ret;
    }

    CRef<objects::CSeq_annot> annot = get_pir_annot(ind, bioseq.GetLength());
    if (annot.NotEmpty())
        bioseq.SetAnnot().push_back(annot);

    TEntryList entries;
    entries.push_back(entry);
    fta_find_pub_explore(pp, entries);

    if (fta_EntryCheckGBBlock(entries))
    {
        ErrPostStr(SEV_WARNING, ERR_ENTRY_GBBlock_not_Empty,
                   "Attention: GBBlock is not empty");
    }

    PackEntries(entries);
    return entry;
}

/**********************************************************/
static DataBlkPtr* SubdbpNew(int size)
{
    int i;

    if(i_subdbp + size > MAXDBP)
    {
        fprintf(stderr, "Too many subdbp's: %d\n", i_subdbp);
        exit(1);
    }
    i = i_subdbp;
    i_subdbp += size;
    return(subdb + i);
}

/**********************************************************/
static DataBlkPtr PirDataBlkNew()
{
    if(i_dbp >= MAXDBP)
    {
        fprintf(stderr, "Too many dbp's: %d\n", i_dbp);
        exit(1);
    }
    db[i_dbp].mpNext = NULL;
    db[i_dbp].mpData = NULL;
    return(db + (i_dbp++));
}

/**********************************************************/
static DataBlkPtr tie_dbp(DataBlkPtr host, DataBlkPtr next)
{
    DataBlkPtr v;

    if(host == NULL)
        return(next);

    for(v = host; v->mpNext != NULL;)
        v = v->mpNext;                    /* empty on purpose */

    v->mpNext = next;
    return(host);
}

/**********************************************************/
static char* save_flat_str(char* str, char* cur, char* end)
{
    size_t l = StringLen(str);
    if(cur + l + 1 >= end)
    {
        fprintf(stderr, "Entry\n");
        exit(1);
    }
    fta_StringCpy(cur, str);
    return(cur + l);
}

/**********************************************************
 *
 *   void GetPirLenSubNode(dbp):
 *
 *      Recalculate the length for the node which has
 *   subkeywords.
 *
 *                                              4-7-93
 *
 **********************************************************/
static void GetPirLenSubNode(DataBlkPtr dbp)
{
    DataBlkPtr curdbp;
    DataBlkPtr predbp;

    if(dbp->mpData == NULL)               /* no sublocks in this block */
        return;

    predbp = dbp;
    curdbp = (DataBlkPtr) dbp->mpData;
    for(; curdbp->mpNext != NULL; curdbp = curdbp->mpNext)
    {
        predbp->len = curdbp->mOffset - predbp->mOffset;
        predbp = curdbp;
    }

    predbp->len = curdbp->mOffset - predbp->mOffset;
}

/**********************************************************/
static void featdbp(DataBlkPtr dbp)
{
    bool       is_fkey;
    bool       is_fkey1;
    bool       is_fkey2;
    bool       is_fkey3;
    char*    bptr;
    char*    eptr;
    char*    ptr;
    DataBlkPtr subdbp;

    if(dbp == NULL)
        return;

    bptr = dbp->mOffset;
    eptr = bptr + dbp->len;

    ptr = SrchTheChar(bptr, eptr, '\n');
    bptr = ptr + 1;

    while(bptr < eptr)
    {
        subdbp = PirDataBlkNew();
        subdbp->mOffset = bptr;
        subdbp->len = eptr - bptr;
        subdbp->mType = FEATUREBLK;
        dbp->mpData = (DataBlkPtr) tie_dbp((DataBlkPtr) dbp->mpData, subdbp);

        do
        {
            is_fkey = false;
            ptr = SrchTheChar(bptr, eptr - 6, '\n');
            if(ptr != NULL)
            {
                is_fkey1 = (SrchTheChar(bptr, ptr, '#') == NULL);
                is_fkey2 = (StringNCmp(bptr, "     ", 5) != 0);
                is_fkey3 = (StringNCmp(ptr, "\n     ", 6) == 0);
                is_fkey = ((is_fkey1 && is_fkey2) || is_fkey3);
                bptr = ptr + 1;
            }
        } while(ptr != NULL && is_fkey);

        if(ptr != NULL)
        {
            bptr = ptr;
            ptr = SrchTheChar(bptr, eptr, '\n');
            bptr = ptr + 1;
        }
        else
            bptr = eptr;
    }

    GetPirLenSubNode(dbp);
}

/**********************************************************/
static void subdbp_func(DataBlkPtr dbp)
{
    DataBlkPtr      tmp;
    DataBlkPtr      subdbp;
    DataBlkPtr* si;
    char*         str;
    const char      **tags;
    int             type;
    int             i_tag;
    int             i;
    char*         s;
    char*         s0;

    if(dbp == NULL)
        return;

    type = 0;
    for(tmp = dbp; tmp != NULL; tmp = tmp->mpNext)
    {
        type = tmp->mType;
        tags = (const char **) sub_tag[type];
        for(i = 0; tags[i] != NULL; i++)
            sub_ind[type][i] = NULL;

        str = tmp->mOffset;
        subdbp = PirDataBlkNew();
        subdbp->mType = 0;
        i_tag = 0;
        if(*str == '#' && str[1] != '#')
            str--;

        subdbp->mOffset = str;
        sub_ind[type][i_tag] = tie_dbp(sub_ind[type][i_tag], subdbp);
        while((s = StringChr(str, '#')) != NULL)
        {
            if(s[1] == '#')
            {
                for(str = s + 2; *str == '#';)
                    str++;
                continue;
            }
            for(s0 = s - 1; s0 > str && isspace(s0[-1]) != 0;)
                s0--;
            *s0 = '\0';
            subdbp->len = s0 - subdbp->mOffset;
            i_tag = fta_StringMatch(tags, s);
            if(i_tag == -1)
            {
                for(str = s; *s != ' ' && *s != '\0' && *s != '\n';)
                    s++;

                *s = '\0';
                ErrPostEx(SEV_ERROR, ERR_FORMAT_Unknown,
                          "Unknown tag: (%s) %s", dbp_tag[type], str);
                str = s + 1;
                continue;
            }
            str = s + StringLen(tags[i_tag]);
            while(isspace((int) *str) != 0)
                str++;
            subdbp = PirDataBlkNew();
            subdbp->mType = i_tag;
            subdbp->mOffset = str;
            sub_ind[type][i_tag] = tie_dbp(sub_ind[type][i_tag], subdbp);
        }
        subdbp->len = tmp->mOffset + tmp->len - subdbp->mOffset;
    }
    si = SubdbpNew(MAXTAG);
    memcpy(si, sub_ind[type], MAXTAG * sizeof(si[0]));
    dbp->mpData = si;
}

/**********************************************************/
static char* fta_get_pir_line(char* line, Int4 len, ParserPtr pp)
{
    const char* p = nullptr;
    char*    q;
    Int4       i;


    auto& fileBuf = pp->ffbuf;
    if(fileBuf.start == NULL || fileBuf.current == NULL ||
       fileBuf.current < fileBuf.start || *fileBuf.start == '\0' ||
       *fileBuf.current == '\0')
        return(NULL);

    for(i = 0, q = line, p = fileBuf.current; i < len && *p != '\0'; i++, p++)
    {
        *q++ = *p;
        if(*p == '\n')
        {
            p++;
            break;
        }
    }
    *q = '\0';
    fileBuf.current = p;
    return(line);
}

/**********************************************************/
bool PirAscii(ParserPtr pp)
{
    char*     beg_str;
    char*     offset;
    int         i;
    int         i_tag;
    int         end_of_file;
    DataBlkPtr  dbp;
    DataBlkPtr  ind[MAXTAG];

    char*     entry_str;
    char*     acc_str;
    char*     s;
    char*     ends;
    Char        entry[MAXMAX];
    Char        flat_str[MAXSTR+1];
    char*     end_entry;
    char*     cur_entry;

    pp->ffbuf.current = pp->ffbuf.start;

    auto protconv = GetProteinConv();        /* set up sequence alphabets
                                           in block.c */

    beg_str = (char*) dbp_tag[ParFlatPIR_ENTRY];
    while(fta_get_pir_line(flat_str, MAXSTR, pp) != NULL)
    {
        if(StringNCmp(flat_str, beg_str, StringLen(beg_str)) == 0)
            break;
    }

    i_tag = ParFlatPIR_ENTRY;
    end_entry = entry + MAXMAX;
    do
    {
        i_dbp = 0;
        cur_entry = entry;
        offset = entry;
        for(i = 0; dbp_tag[i] != NULL; i++)
            ind[i] = NULL;

        do
        {
            dbp = PirDataBlkNew();
            dbp->mType = i_tag;
            size_t ioff = StringLen(dbp_tag[i_tag]);
            while(flat_str[ioff] == ' ')
                ioff++;
            dbp->mOffset = offset + ioff;
            ind[i_tag] = tie_dbp(ind[i_tag], dbp);
            do
            {
                cur_entry = save_flat_str(flat_str, cur_entry, end_entry);
                offset = cur_entry;
                if(fta_get_pir_line(flat_str, MAXSTR, pp) == NULL)
                {
                    end_of_file = 1;
                    break;
                }
                end_of_file = 0;

                i_tag = fta_StringMatch(dbp_tag, flat_str);
            } while(end_of_file == 0 && i_tag == -1);
            dbp->len = offset - dbp->mOffset;
            if(dbp->mType == ParFlatPIR_ACCESSIONS)
            {
                acc_str = tata_save(dbp->mOffset);
                ends = acc_str + 12;
                s = acc_str;
                while(s < ends && *s != '\0' && *s != ' ' && *s != ';')
                    s++;
                *s = '\0';
                if(acc_str == NULL || *acc_str == '\0')
                    FtaInstallPrefix(PREFIX_ACCESSION, (char *) "???", NULL);
                else
                    FtaInstallPrefix(PREFIX_ACCESSION, acc_str, NULL);
                if(acc_str != NULL)
                    MemFree(acc_str);
            }
            if(dbp->mType == ParFlatPIR_FEATURE)
            {
                featdbp(dbp);
            }
            else
            {
                subdbp_func(dbp);
            }
            if(dbp->mType == ParFlatPIR_ENTRY)
            {
                entry_str = tata_save(dbp->mOffset);
                ends = entry_str + 12;
                s = entry_str;
                while(s < ends && *s != '\0' && *s != ' ' && *s != ';')
                    s++;
                *s = '\0';
                FtaInstallPrefix(PREFIX_LOCUS, entry_str, NULL);
                MemFree(entry_str);
            }
            cur_entry++;
            offset++;
        } while(end_of_file == 0 && i_tag != ParFlatPIR_ENTRY);

        CRef<objects::CSeq_entry> cur_entry = ind2asn(pp, ind, sub_ind, protconv.get());
        if (cur_entry.Empty())
        {
            ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                      "Entry skipped: \"%s|%s\".",
                      pp->entrylist[pp->curindx]->locusname,
                      pp->entrylist[pp->curindx]->acnum);
        }
        else
        {
            pp->entries.push_back(cur_entry);
            ErrPostStr(SEV_INFO, ERR_ENTRY_Parsed, "OK");
        }
        i_subdbp = 0;

    } while(end_of_file == 0);

    ErrPostStr(SEV_INFO, ERR_ENTRY_Parsed, "Parsing completed");

    return true;
}

END_NCBI_SCOPE
// LCOV_EXCL_STOP
