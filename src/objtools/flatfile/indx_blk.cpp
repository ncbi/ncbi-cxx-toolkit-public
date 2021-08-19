/* indx_blk.c
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
 * File Name:  indx_blk.c
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *      Common for all format functions.
 *
 */
#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include "index.h"
#include <objtools/flatfile/flatfile_parse_info.hpp>

#include "ftaerr.hpp"
#include "indx_blk.h"
#include "indx_def.h"
#include "utilfun.h"
#include <map>

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "indx_blk.cpp"


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
static const char *XML_STRAND_array[] = {
    "   ", "single", "double", "mixed", NULL
};

static const char *XML_TPG_array[] = {
    "   ", "Linear", "Circular", "Tandem", NULL
};

static const char *ParFlat_STRAND_array[] = {
    "   ", "ss-", "ds-", "ms-", NULL
};

static const char *ParFlat_TPG_array[] = {
    "         ", "Linear   ", "Circular ", "Tandem   ", NULL
};

static const char *ParFlat_NA_array_DDBJ[] = {
    "cDNA", NULL
};

static const char *ParFlat_AA_array_DDBJ[] = {
    "PRT", NULL
};

static const char *ParFlat_NA_array[] = {
    "    ", "NA", "DNA", "genomic DNA", "other DNA", "unassigned DNA", "RNA",
    "mRNA", "rRNA", "tRNA", "uRNA", "scRNA", "snRNA", "snoRNA", "pre-RNA",
    "pre-mRNA", "genomic RNA", "other RNA", "unassigned RNA", "cRNA",
    "viral cRNA",  NULL
};

static const char *ParFlat_DIV_array[] = {
    "   ", "PRI", "ROD", "MAM", "VRT", "INV", "PLN", "BCT", "RNA",
    "VRL", "PHG", "SYN", "UNA", "EST", "PAT", "STS", "ORG", "GSS",
    "HUM", "HTG", "CON", "HTC", "ENV", "TSA", NULL
};

static const char *embl_accpref[] = {
    "AJ", "AL", "AM", "AN", "AX", "BN", "BX", "CQ", "CR", "CS", "CT", "CU",
    "FB", "FM", "FN", "FO", "FP", "FQ", "FR", "GM", "GN", "HA", "HB", "HC",
    "HD", "HE", "HF", "HG", "HH", "HI", "JA", "JB", "JC", "JD", "JE", "LK",
    "LL", "LM", "LN", "LO", "LP", "LQ", "LR", "LS", "LT", "MP", "MQ", "MR",
    "MS", "OA", "OB", "OC", "OD", "OE", "OU", "OV", "OW", "OX", "OY", "OZ",
    NULL
};

static const char *lanl_accpref[] = {
    "AD", NULL
};

static const char *pir_accpref[] = {
    "CC", NULL
};

static const char *prf_accpref[] = {
    "XX", NULL
};

static const char *sprot_accpref[] = {
    "DD", NULL
};

static const char *ddbj_accpref[] = {
    "AB", "AG", "AK", "AP", "AT", "AU", "AV", "BA", "BB", "BD", "BJ", "BP",
    "BR", "BS", "BW", "BY", "CI", "CJ", "DA", "DB", "DC", "DD", "DE", "DF",
    "DG", "DH", "DI", "DJ", "DK", "DL", "DM", "FS", "FT", "FU", "FV", "FW",
    "FX", "FY", "FZ", "GA", "GB", "HT", "HU", "HV", "HW", "HX", "HY", "HZ",
    "LA", "LB", "LC", "LD", "LE", "LF", "LG", "LH", "LI", "LJ", "LU", "LV",
    "LX", "LY", "LZ", "MA", "MB", "MC", "MD", "ME", "OF", "OG", "OH", "OI",
    "OJ", "PA", NULL
};

static const char *ncbi_accpref[] = {
    "AA", "AC", "AD", "AE", "AF", "AH", "AI", "AQ", "AR", "AS", "AW", "AY",
    "AZ", "BC", "BE", "BF", "BG", "BH", "BI", "BK", "BL", "BM", "BQ", "BT",
    "BU", "BV", "BZ", "CA", "CB", "CC", "CD", "CE", "CF", "CG", "CH", "CK",
    "CL", "CM", "CN", "CO", "CP", "CV", "CW", "CX", "CY", "CZ", "DN", "DP",
    "DQ", "DR", "DS", "DT", "DU", "DV", "DW", "DX", "DY", "DZ", "EA", "EB",
    "EC", "ED", "EE", "EF", "EG", "EH", "EI", "EJ", "EK", "EL", "EM", "EN",
    "EP", "EQ", "ER", "ES", "ET", "EU", "EV", "EW", "EX", "EY", "EZ", "FA",
    "FC", "FD", "FE", "FF", "FG", "FH", "FI", "FJ", "FK", "FL", "GC", "GD",
    "GE", "GF", "GG", "GH", "GJ", "GK", "GL", "GO", "GP", "GQ", "GR", "GS",
    "GT", "GU", "GV", "GW", "GX", "GY", "GZ", "HJ", "HK", "HL", "HM", "HN",
    "HO", "HP", "HQ", "HR", "HS", "JF", "JG", "JH", "JI", "JJ", "JK", "JL",
    "JM", "JN", "JO", "JP", "JQ", "JR", "JS", "JT", "JU", "JV", "JW", "JX",
    "JY", "JZ", "KA", "KB", "KC", "KD", "KE", "KF", "KG", "KH", "KI", "KJ",
    "KK", "KL", "KM", "KN", "KO", "KP", "KQ", "KR", "KS", "KT", "KU", "KV",
    "KX", "KY", "KZ", "MF", "MG", "MH", "MI", "MJ", "MK", "ML", "MM", "MN",
    "MO", "MT", "MU", "MV", "MW", "MX", "MY", "MZ", "OK", "OL", "OM", "ON",
    "OT", NULL
};

static const char *refseq_accpref[] = {
    "NC_", "NG_", "NM_", "NP_", "NR_", "NT_", "NW_", "XM_", "XP_", "XR_",
    "NZ_", NULL
};

static const char *refseq_prot_accpref[] = {
    "AP_", "NP_", "WP_", "XP_", "YP_", "ZP_", NULL
};

static const char *acc_tsa_allowed[] = {
    "AF", "AY", "DQ", "EF", "EU", "FJ", "GQ", "HQ", "JF", "JN", "JQ", "JX",
    "KC", "KF", "KJ", "KM", "KP", "KR", "KT", "KU", "KX", "KY", "MF", "MG",
    "MH", "MK", "MN", "MT", NULL
};

static const char *ncbi_tpa_accpref[] = {
    "BK", "BL", "GJ", "GK", NULL
};

static const char *ddbj_tpa_accpref[] = {
    "BR", "HT", "HU", NULL
};

static const char *ncbi_wgs_accpref[] = {
    "GJ", "GK", NULL
};

static const char *ddbj_wgs_accpref[] = {
    "HT", "HU", NULL
};

static const set<string> k_WgsScaffoldPrefix =
    {"CH", "CT", "CU", "DF", "DG", "DS",
     "EM", "EN", "EP", "EQ", "FA", "FM",
     "GG", "GJ", "GK", "GL", "HT", "HU",
     "JH", "KB", "KD", "KE", "KI", "KK",
     "KL", "KN", "KQ", "KV", "KZ", "LD",
     "ML", "MU"};

//static const char *wgs_scfld_pref[] = 

static const char *source[11] = {
    "unknown",
    "EMBL",
    "GENBANK",
    "PIR",
    "Swiss-Prot",
    "NCBI",
    "GSDB",
    "DDBJ",
    "FlyBase",
    "RefSeq",
    "unknown"
};


static const map<Parser::ESource, string> sourceNames =  {
    {Parser::ESource::unknown, "unknown"},
    {Parser::ESource::EMBL, "EMBL"},
    {Parser::ESource::GenBank, "GENBANK"},
    {Parser::ESource::PIR, "PIR"},
    {Parser::ESource::SPROT, "Swiss-Prot"},
    {Parser::ESource::NCBI, "NCBI"},
    {Parser::ESource::LANL, "GSDB"},
    {Parser::ESource::Flybase, "FlyBase"},
    {Parser::ESource::Refseq, "RefSeq"},
    {Parser::ESource::PRF, "unknown"}};

static const char *month_name[] = {
    "Ill", "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
    "JUL", "AUG", "SEP", "OCT", "NOV", "DEC", NULL
};

static const char *ParFlat_RESIDUE_STR[] = {
    "bp", "bp.", "bp,", "AA", "AA.", "AA,", NULL
};

static const char *ValidMolTypes[] = {
    "genomic DNA",
    "genomic RNA",
    "mRNA",
    "tRNA",
    "rRNA",
    "snoRNA",
    "snRNA",
    "scRNA",
    "pre-RNA",
    "pre-mRNA",
    "other RNA",
    "other DNA",
    "transcribed RNA",
    "unassigned RNA",
    "unassigned DNA",
    "viral cRNA",
    NULL
};

// functions below are implemented in different source files
bool EmblIndex(ParserPtr pp, void (*fun)(IndexblkPtr entry, char* offset, Int4 len));
bool GenBankIndex(ParserPtr pp);
bool SprotIndex(ParserPtr pp, void(*fun)(IndexblkPtr entry, char* offset, Int4 len));
bool PrfIndex(ParserPtr pp, void(*fun)(IndexblkPtr entry, char* offset, Int4 len));
bool PirIndex(ParserPtr pp, void(*fun)(IndexblkPtr entry, char* offset, Int4 len));
bool XMLIndex(ParserPtr pp);

/**********************************************************
 *
 *   static char* GetResidue(stoken):
 *
 *      Return a string pointer in the "stoken" which its
 *   next token string match any one string in the
 *   ParFlat_RESIDUE_STR but ignore case for all alphabetic
 *   characters; return NULL if not found.
 *
 *                                              3-25-93
 *
 **********************************************************/
static char* GetResidue(TokenStatBlkPtr stoken)
{
    TokenBlkPtr  sptr;
    TokenBlkPtr  ptr;
    const char   **b;
    Int2         i;

    ptr = stoken->list;
    sptr = stoken->list->next;
    for(i = 1; i < stoken->num; i++, ptr = ptr->next, sptr = sptr->next)
    {
        for(b = ParFlat_RESIDUE_STR; *b != NULL; b++)
            if(StringICmp(*b, sptr->str) == 0)
                return(ptr->str);
    }

    return(NULL);
}

/**********************************************************
 *
 *   bool XReadFile(fp, finfo):
 *
 *      Record position and line # of the file, loop stop
 *   when got a none blank line.
 *      Return TRUE if END_OF_FILE.
 *
 *                                              2-26-93
 *
 **********************************************************/
bool XReadFile(FILE* fp, FinfoBlkPtr finfo)
{
    bool end_of_file = false;

    StringCpy(finfo->str, "\n");
    while(!end_of_file && StringNCmp(finfo->str, "\n", 1) == 0)
    {
        finfo->pos = (size_t) ftell(fp);
        if (fgets(finfo->str, sizeof(finfo->str) - 1, fp) == NULL)
            end_of_file = true;
        else
            ++(finfo->line);
    }

    auto n = strlen(finfo->str);
    while (n) {
        n--;
        if (finfo->str[n] != '\n' && finfo->str[n] != '\r') {
            break;
        }
        finfo->str[n] = 0;
    }

    return(end_of_file);
}

/**********************************************************/
static Int2 FileGetsBuf(char* res, Int4 size, FileBuf& fbuf)
{
    const char* p = nullptr;
    char* q;
    Int4    l;
    Int4    i;

    if(*fbuf.current == '\0')
        return(0);

    l = size - 1;
    for(p = fbuf.current, q = res, i = 0; i < l; i++, p++)
    {
        *q++ = *p;
        if(*p == '\n' || *p == '\r')
        {
            p++;
            break;
        }
    }

    *q = '\0';
    fbuf.current = p;
    return(1);
}

/**********************************************************/
bool XReadFileBuf(FileBuf& fbuf, FinfoBlkPtr finfo)
{
    bool end_of_file = false;

    StringCpy(finfo->str, "\n");
    while(!end_of_file && StringNCmp(finfo->str, "\n", 1) == 0)
    {
        finfo->pos = (size_t) (fbuf.current - fbuf.start);
        if(FileGetsBuf(finfo->str, sizeof(finfo->str) - 1, fbuf) == 0)
            end_of_file = true;
        else
            ++(finfo->line);
    }

    return(end_of_file);
}

/**********************************************************
 *
 *   bool SkipTitle(fp, finfo, str, len):
 *
 *      Return TRUE if file contains no entry in which no
 *   match in keyword "str".
 *      Skip any title declaration lines.
 *
 *                                              3-5-93
 *
 **********************************************************/
bool SkipTitle(FILE* fp, FinfoBlkPtr finfo, const char *str, Int2 len)
{
    bool end_of_file = XReadFile(fp, finfo);
    while(!end_of_file && StringNCmp(finfo->str, str, len) != 0)
        end_of_file = XReadFile(fp, finfo);

    return(end_of_file);
}


bool SkipTitle(FILE* fp, FinfoBlkPtr finfo, const CTempString& keyword)
{
    return SkipTitle(fp, finfo, keyword.data(), keyword.size());
}

/**********************************************************/
bool SkipTitleBuf(FileBuf& fbuf, FinfoBlkPtr finfo, const char *str, Int2 len)
{
    bool end_of_file = XReadFileBuf(fbuf, finfo);
    while(!end_of_file && StringNCmp(finfo->str, str, len) != 0)
        end_of_file = XReadFileBuf(fbuf, finfo);

    return(end_of_file);
}


bool SkipTitleBuf(FileBuf& fbuf, FinfoBlkPtr finfo, const CTempString& keyword)
{
    return SkipTitleBuf(fbuf, finfo, keyword.data(), keyword.size());
}


/**********************************************************
 *
 *   static bool CheckLocus(locus):
 *
 *      Locus name only allow A-Z, 0-9, characters,
 *   reject if not.
 *
 **********************************************************/
static bool CheckLocus(char* locus, Parser::ESource source)
{
    char* p = locus;
    if(StringNCmp(locus, "SEG_", 4) == 0 &&
       (source == Parser::ESource::NCBI || source == Parser::ESource::DDBJ))
        p += 4;
    for(; *p != '\0'; p++)
    {
        if((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'Z') ||
           (*p == '.' && source == Parser::ESource::Flybase))
            continue;
        if(((*p >= 'a' && *p <= 'z') || *p == '_' || *p == '-' || *p == '(' ||
             *p == ')' || *p == '/') && source == Parser::ESource::Refseq)
            continue;

        ErrPostEx(SEV_ERROR, ERR_LOCUS_BadLocusName,
                  "Bad locusname, <%s> for this entry", locus);
        break;
    }

    return (*p != '\0');
}

/**********************************************************
 *
 *   static bool CheckLocusSP(locus):
 *
 *      Locus name consists of up tp 10 uppercase
 *   alphanumeric characters.
 *      Rule: X_Y format (SWISS-PROT), reject if not
 *      - X is a mnemonic code, up to 4 alphanumeric
 *        characters to represent the protein name.
 *      - Y is a mnemonic species identification code of
 *        at most 5 alphanumeric characters to representing
 *        the biological source of the protein.
 *      Checking the defined species identification code
 *   has not been implemented.
 *
 *      Example:  RL1_ECOLI   FER_HALHA
 *
 **********************************************************/
static bool CheckLocusSP(char* locus)
{
    char* p;
    bool underscore = false;
    Int2    x;
    Int2    y;

    for(p = locus, x = y = 0; *p != '\0'; p++)
    {
        if((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'Z'))
        {
            if (!underscore)
                x++;
            else
                y++;
        }
        else if(*p == '_')
            underscore = true;
        else
            break;
    }

    if(*p != '\0' || x == 0 || y == 0)
    {
        ErrPostEx(SEV_ERROR, ERR_LOCUS_BadLocusName,
                  "Bad locusname, <%s> for this entry", locus);
        return true;
    }

    return false;
}

/**********************************************************
 *
 *   static bool CkDateFormat(date):
 *
 *      Return FALSE if date != dd-mmm-yyyy format.
 *
 **********************************************************/
static bool CkDateFormat(char* date)
{
    if(date[2] == '-' && date[6] == '-' &&
       IS_DIGIT(date[0]) != 0 && IS_DIGIT(date[1]) != 0 &&
       IS_DIGIT(date[7]) != 0 && IS_DIGIT(date[8]) != 0 &&
       IS_DIGIT(date[9]) != 0 && IS_DIGIT(date[10]) != 0 &&
       MatchArraySubString(month_name, date) != -1)
        return true;

    return false;
}

/**********************************************************/
Int2 CheckSTRAND(const char* str)
{
    return(fta_StringMatch(ParFlat_STRAND_array, str));
}

/**********************************************************/
Int2 XMLCheckSTRAND(char* str)
{
    return(StringMatchIcase(XML_STRAND_array, str));
}

/**********************************************************/
Int2 XMLCheckTPG(char* str)
{
    Int2 i;

    i = StringMatchIcase(XML_TPG_array, str);
    if(i == 0)
        i++;
    return(i);
}

/**********************************************************/
Int2 CheckTPG(char* str)
{
    return(StringMatchIcase(ParFlat_TPG_array, str));
}

/**********************************************************/
Int2 CheckNADDBJ(char* str)
{
    return(fta_StringMatch(ParFlat_NA_array_DDBJ, str));
}

/**********************************************************/
Int2 CheckNA(char* str)
{
    return(fta_StringMatch(ParFlat_NA_array, str));
}

/**********************************************************/
Int2 CheckDIV(char* str)
{
    return(fta_StringMatch(ParFlat_DIV_array, str));
}

/**********************************************************/
bool CkLocusLinePos(char* offset, Parser::ESource source, LocusContPtr lcp, bool is_mga)
{
    Char    date[12];
    bool ret = true;
    char* p;
    Int4    i;

    p = StringChr(offset, '\n');
    if(p != NULL)
        *p = '\0';

    if(is_mga == false && StringNCmp(offset + lcp->bp, "bp", 2) != 0 &&
       StringNCmp(offset + lcp->bp, "rc", 2) != 0 &&
       StringNCmp(offset + lcp->bp, "aa", 2) != 0)
    {
        i = lcp->bp + 1;
        ErrPostEx(SEV_WARNING, ERR_FORMAT_LocusLinePosition,
                  "bp/rc string unrecognized in column %d-%d: %s",
                  i, i + 1, offset + lcp->bp);
        ret = false;
    }
    if(CheckSTRAND(offset + lcp->strand) == -1)
    {
        i = lcp->strand + 1;
        ErrPostEx(SEV_WARNING, ERR_FORMAT_LocusLinePosition,
                  "Strand unrecognized in column %d-%d : %s",
                  i, i + 2, offset + lcp->strand);
    }

    p = offset + lcp->molecule;
    if(is_mga)
    {
        if(StringNICmp(p, "mRNA", 4) != 0 && StringNCmp(p, "RNA", 3) != 0)
        {
            ErrPostEx(SEV_REJECT, ERR_FORMAT_IllegalCAGEMoltype,
                      "Illegal molecule type provided in CAGE record in LOCUS line: \"%s\". Must be \"mRNA\"or \"RNA\". Entry dropped.",
                      p);
            ret = false;
        }
    }
    else if(StringMatchIcase(ParFlat_NA_array, p) == -1)
    {
        if(StringMatchIcase(ParFlat_AA_array_DDBJ, p) == -1)
        {
            i = lcp->molecule + 1;
            if(source != Parser::ESource::DDBJ ||
               StringMatchIcase(ParFlat_NA_array_DDBJ, p) == -1)
            {
                ErrPostEx(SEV_WARNING, ERR_FORMAT_LocusLinePosition,
                          "Molecule unrecognized in column %d-%d: %s",
                          i, i + 5, p);
                ret = false;
            }
        }
    }

    if(CheckTPG(offset + lcp->topology) == -1)
    {
        i = lcp->topology + 1;
        ErrPostEx(SEV_WARNING, ERR_FORMAT_LocusLinePosition,
                  "Topology unrecognized in column %d-%d: %s",
                  i, i + 7, offset + lcp->topology);
        ret = false;
    }
    if(CheckDIV(offset + lcp->div) == -1)
    {
        i = lcp->div + 1;
        ErrPostEx(SEV_WARNING, ERR_FORMAT_LocusLinePosition,
                  "Division code unrecognized in column %d-%d: %s",
                  i, i + 2, offset + lcp->div);
        ret = (source == Parser::ESource::LANL);
    }
    MemCpy(date, offset + lcp->date, 11);
    date[11] = '\0';
    if(StringNCmp(date, "NODATE", 6) == 0)
    {
        ErrPostEx(SEV_WARNING, ERR_FORMAT_LocusLinePosition,
                  "NODATE in LOCUS line will be replaced by current system date");
    }
    else if(!CkDateFormat(date))
    {
        i = lcp->date + 1;
        ErrPostEx(SEV_WARNING, ERR_FORMAT_LocusLinePosition,
                  "Date should be in column %d-%d, and format dd-mmm-yyyy: %s",
                  i, i + 10, date);
        ret = false;
    }

    if(p != NULL)
        *p = '\n';
    return(ret);
}

/**********************************************************
    *
    *   CRef<objects::CDate_std> GetUpdateDate(ptr, source):
    *
    *      Return NULL if ptr does not have dd-mmm-yyyy format
    *   or "NODATE"; otherwise, return Date-std pointer.
    *
    **********************************************************/
CRef<objects::CDate_std> GetUpdateDate(char* ptr, Parser::ESource source)
{
    Char date[12];

    if (StringNCmp(ptr, "NODATE", 6) == 0)
        return CRef<objects::CDate_std>(new objects::CDate_std(CTime(CTime::eCurrent)));

    if (ptr[11] != '\0' && ptr[11] != '\n' && ptr[11] != ' ' &&
        (source != Parser::ESource::SPROT || ptr[11] != ','))
        return CRef<objects::CDate_std>();

    MemCpy(date, ptr, 11);
    date[11] = '\0';

    if (!CkDateFormat(date))
        return CRef<objects::CDate_std>();

    return get_full_date(ptr, false, source);
}


/**********************************************************/
static bool fta_check_embl_moltype(char* str)
{
    const char **b;
    char*    p;
    char*    q;

    p = StringChr(str, ';');
    p = StringChr(p + 1, ';');
    p = StringChr(p + 1, ';');

    for(p++; *p == ' ';)
       p++;

    q = StringChr(p, ';');
    *q = '\0';

    for(b = ValidMolTypes; *b != NULL; b++)
        if(StringCmp(p, *b) == 0)
            break;

    if(*b != NULL)
    {
        *q = ';';
        return true;
    }

    ErrPostEx(SEV_REJECT, ERR_FORMAT_InvalidIDlineMolType,
              "Invalid moltype value \"%s\" provided in ID line of EMBL record.",
              p);
    *q = ';';
    return false;
}

/*********************************************************
indexblk_struct constructor
**********************************************************/
indexblk_struct::indexblk_struct() :
    vernum(0),
    offset(0),
    bases(0),
    segnum(0),
    segtotal(0),
    linenum(0),
    drop(0),
    len(0),
    EST(false),
    STS(false),
    GSS(false),
    HTC(false),
    htg(0),
    is_contig(false),
    is_mga(false),
    origin(false),
    is_pat(false),
    is_wgs(false),
    is_tpa(false),
    is_tsa(false),
    is_tls(false),
    is_tpa_wgs_con(false),
    tsa_allowed(false),
    moltype(NULL),
    gaps(NULL),
    secaccs(NULL),
    xip(NULL),
    embl_new_ID(false),
    env_sample_qual(false),
    is_prot(false),
    organism(NULL),
    taxid(0),
    no_gc_warning(false),
    qsoffset(0),
    qslength(0),
    wgs_and_gi(0),
    got_plastid(false),
    gc_genomic(0),
    gc_mito(0),
    specialist_db(false),
    inferential(false),
    experimental(false),
    submitter_seqid(NULL),
    ppp(NULL)
{
    acnum[0] = 0;
    locusname[0] = 0;
    division[0] = 0;
    blocusname[0] = 0;

    MemSet(&lc, 0, sizeof(lc));

    wgssec[0] = 0;
}

static bool isSpace(char c) 
{
    return isspace(c);
}


static CTempString::const_iterator 
sFindNextSpace(const CTempString& tempString, 
        CTempString::const_iterator current_it)
{
    return find_if(current_it, tempString.end(), isSpace);
}


static CTempString::const_iterator 
sFindNextNonSpace(const CTempString& tempString, 
        CTempString::const_iterator current_it)
{
    return find_if_not(current_it, tempString.end(), isSpace);
}


static void sSetLocusLineOffsets(const CTempString& locusLine, LocusCont& offsets) 
{
    offsets.bases = -1;
    offsets.bp = -1;
    offsets.strand = -1;
    offsets.molecule = -1;
    offsets.topology = -1;
    offsets.div = -1;
    offsets.date = -1;

    if (locusLine.substr(0,5) != "LOCUS") {
        // throw an exception - invalid locus line
    }


    auto it = sFindNextNonSpace(locusLine, locusLine.begin()+5);
    if (it == locusLine.end()) {
        // throw an exception - no locus name
    }

    it = sFindNextSpace(locusLine, it);
    if (it == locusLine.end()) {
        return;
    }

    // find the number of bases
    it = sFindNextNonSpace(locusLine, it);
    if (it == locusLine.end()) {
        return;
    }
    auto space_it = sFindNextSpace(locusLine, it);
    if (NStr::StringToNonNegativeInt(locusLine.substr(it-begin(locusLine), space_it-it)) == -1) {
        return;
    }

    offsets.bases = it - begin(locusLine);

    it = sFindNextNonSpace(locusLine, space_it);
    offsets.bp = it - begin(locusLine);

    it = sFindNextSpace(locusLine, it);
    it = sFindNextNonSpace(locusLine, it);
    // the next one might be a strand
    // or might be a molecule
    space_it = sFindNextSpace(locusLine, it);
    offsets.strand = -1;
    if ((space_it - it)==3) {
        auto currentSubstr = locusLine.substr(it-begin(locusLine),3);
        if (currentSubstr=="ss-" ||
            currentSubstr=="ds-" ||
            currentSubstr=="ms-") {
            offsets.strand = it - begin(locusLine);
            it = sFindNextNonSpace(locusLine, space_it);
        }
        offsets.molecule = it - begin(locusLine);
    }
    else {
        offsets.molecule = it - begin(locusLine);
    }

    // topology
    it = sFindNextSpace(locusLine, it);
    it = sFindNextNonSpace(locusLine, it);
    if (it != locusLine.end()) {
        offsets.topology = it - begin(locusLine); 
    }

    // find division
    it = sFindNextSpace(locusLine, it);
    it = sFindNextNonSpace(locusLine, it);
    if (it != locusLine.end()) {
        offsets.div = it - begin(locusLine);
    }

    // find date - date is optional
    it = sFindNextSpace(locusLine, it);
    it = sFindNextNonSpace(locusLine, it);
    if (it != locusLine.end()) {
        offsets.date = it - begin(locusLine);
    } 
}

/**********************************************************
 *
 *   IndexblkPtr InitialEntry(pp, finfo):
 *
 *      Assign the entry's value to offset, locusname,
 *   bases, linenum, drop blocusname.
 *      Swiss-prot locusname checking is different from
 *   others.
 *      Check LOCUS line column position, genbank format.
 *
 **********************************************************/
IndexblkPtr InitialEntry(ParserPtr pp, FinfoBlkPtr finfo)
{
    Int2            i;
    Int2            j;
    TokenStatBlkPtr stoken;
    TokenBlkPtr     ptr;
    char*         bases;
    IndexblkPtr     entry;
    char*         p;

    entry = new Indexblk;

    entry->offset = finfo->pos;
    entry->linenum = finfo->line;
    entry->ppp = pp;
    entry->is_tsa = false;
    entry->is_tls = false;
    entry->is_pat = false;

    if(pp->source == Parser::ESource::PRF)
        stoken = TokenString(finfo->str, ';');
    else
        stoken = TokenString(finfo->str, ' ');

    bool badlocus = false;
    if(stoken->num > 2 || (pp->format == Parser::EFormat::PRF && stoken->num > 1))
    {
        p = finfo->str;
        if (pp->mode == Parser::EMode::Relaxed) {
            sSetLocusLineOffsets(p, entry->lc);
        } else {
            if(StringLen(p) > 78 && p[28] == ' ' && p[63] == ' ' && p[67] == ' ')
            {
                entry->lc.bases = ParFlat_COL_BASES_NEW;
                entry->lc.bp = ParFlat_COL_BP_NEW;
                entry->lc.strand = ParFlat_COL_STRAND_NEW;
                entry->lc.molecule = ParFlat_COL_MOLECULE_NEW;
                entry->lc.topology = ParFlat_COL_TOPOLOGY_NEW;
                entry->lc.div = ParFlat_COL_DIV_NEW;
                entry->lc.date = ParFlat_COL_DATE_NEW;
            }
            else
            {
                entry->lc.bases = ParFlat_COL_BASES;
                entry->lc.bp = ParFlat_COL_BP;
                entry->lc.strand = ParFlat_COL_STRAND;
                entry->lc.molecule = ParFlat_COL_MOLECULE;
                entry->lc.topology = ParFlat_COL_TOPOLOGY;
                entry->lc.div = ParFlat_COL_DIV;
                entry->lc.date = ParFlat_COL_DATE;
            }
        }

        ptr = stoken->list->next;
        if(pp->format == Parser::EFormat::EMBL && ptr->next != NULL &&
           ptr->next->str != NULL && StringCmp(ptr->next->str, "SV") == 0)
        {
            for(i = 0, p = finfo->str; *p != '\0'; p++)
                if(*p == ';' && p[1] == ' ')
                    i++;

            entry->embl_new_ID = true;
            p = StringRChr(ptr->str, ';');
            if(p != NULL && p[1] == '\0')
                *p = '\0';

            FtaInstallPrefix(PREFIX_LOCUS, ptr->str, NULL);
            FtaInstallPrefix(PREFIX_ACCESSION, ptr->str, NULL);

            if(i != 6 || (stoken->num != 10 && stoken->num != 11))
            {
                ErrPostEx(SEV_REJECT, ERR_FORMAT_BadlyFormattedIDLine,
                          "The number of fields in this EMBL record's new ID line does not fit requirements.");
                badlocus = true;
            }
            else if(fta_check_embl_moltype(finfo->str) == false)
                badlocus = true;
        }

        StringCpy(entry->locusname, ptr->str);
        StringCpy(entry->blocusname, entry->locusname);
        if(pp->format == Parser::EFormat::PIR || pp->format == Parser::EFormat::PRF)
            StringCpy(entry->acnum, entry->locusname);

        if(entry->embl_new_ID == false)
        {
            FtaInstallPrefix(PREFIX_LOCUS, entry->locusname, NULL);
            FtaInstallPrefix(PREFIX_ACCESSION, entry->locusname, NULL);
        }

        if(pp->mode != Parser::EMode::Relaxed && !badlocus)
        {
            if(pp->format == Parser::EFormat::SPROT)
            {
                if(ptr->next == NULL || ptr->next->str == NULL ||
                   (StringNICmp(ptr->next->str, "preliminary", 11) != 0 &&
                    StringNICmp(ptr->next->str, "unreviewed", 10) != 0))
                    badlocus = CheckLocusSP(entry->locusname);
                else
                    badlocus = false;
            }
            else if(pp->format == Parser::EFormat::PIR || pp->format == Parser::EFormat::PRF)
                badlocus = false;
            else 
                badlocus = CheckLocus(entry->locusname, pp->source);
        }
    }
    else if (pp->mode != Parser::EMode::Relaxed) 
    {
        badlocus = true;
        ErrPostStr(SEV_ERROR, ERR_LOCUS_NoLocusName,
                   "No locus name for this entry");
    }

    if(badlocus)
    {
        p = StringChr(finfo->str, '\n');
        if(p != NULL)
            *p = '\0';
        ErrPostEx(SEV_ERROR, ERR_ENTRY_Skipped,
                  "Entry skipped. LOCUS line = \"%s\".", finfo->str);
        if(p != NULL)
            *p = '\n';
        MemFree(entry);
        FreeTokenstatblk(stoken);
        return(NULL);
    }

    if(pp->format == Parser::EFormat::PIR || pp->format == Parser::EFormat::PRF)
    {
        FreeTokenstatblk(stoken);
        return(entry);
    }

    bases = GetResidue(stoken);
    if(bases != NULL)
        entry->bases = (size_t) atoi(bases);

    if(pp->format == Parser::EFormat::GenBank &&
       entry->lc.date > -1)
    {
        /* last token in the LOCUS line is date of the update's data
         */
        for(i = 1, ptr = stoken->list; i < stoken->num; i++)
            ptr = ptr->next;
        entry->date = GetUpdateDate(ptr->str, pp->source);
    }

    if(pp->source == Parser::ESource::DDBJ || pp->source == Parser::ESource::EMBL)
    {
        j = stoken->num - ((pp->format == Parser::EFormat::GenBank) ? 2 : 3);
        for(i = 1, ptr = stoken->list; i < j; i++)
            ptr = ptr->next;

        if(pp->format == Parser::EFormat::EMBL)
        {
            if(StringNICmp(ptr->str, "TSA", 3) == 0)
                entry->is_tsa = true;
            else if(StringNICmp(ptr->str, "PAT", 3) == 0)
                entry->is_pat = true;
        }

        ptr = ptr->next;

        if(StringNICmp(ptr->str, "EST", 3) == 0)
            entry->EST = true;
        else if(StringNICmp(ptr->str, "STS", 3) == 0)
            entry->STS = true;
        else if(StringNICmp(ptr->str, "GSS", 3) == 0)
            entry->GSS = true;
        else if(StringNICmp(ptr->str, "HTC", 3) == 0)
            entry->HTC = true;
        else if(StringNICmp(ptr->str, "PAT", 3) == 0 &&
                pp->source == Parser::ESource::EMBL)
            entry->is_pat = true;
    }
    FreeTokenstatblk(stoken);

    return(entry);
}

/**********************************************************
 *
 *   void DelNoneDigitTail(str):
 *
 *      Delete any non digit characters from tail
 *   of string "str".
 *
 *                                              3-25-93
 *
 **********************************************************/
void DelNoneDigitTail(char* str)
{
    char* p;

    if(str == NULL || *str == '\0')
        return;

    for(p = str; *str != '\0'; str++)
        if(*str >= '0' && *str <= '9')
            p = str + 1;

    *p = '\0';
}
/*
static void sDelNonDigitTail(string& str)
{
    if (str.empty()) {
        return;
    }
    auto nondigitPos = str.find_first_not_of("0123456789");
    if (nondigitPos != string::npos) {
        str = str.substr(0,nondigitPos);
    }
}
*/

/**********************************************************
 *
 * Here X is an alpha character, N - numeric one.
 * Return values:
 *
 * 1 - XXN      (AB123456)
 * 2 - XX_N     (NZ_123456)
 * 3 - XXXXN    (AAAA01000001)
 * 4 - XX_XXXXN (NZ_AAAA01000001)
 * 5 - XXXXXN   (AAAAA1234512)
 * 6 - XX_XXN   (NZ_AB123456)
 * 7 - XXXXNNSN (AAAA01S000001 - scaffolds)
 * 8 - XXXXXXN  (AAAAAA010000001)
 * 0 - all others
 *
 */
Int4 IsNewAccessFormat(const Char* acnum)
{
    const Char* p = acnum;

    if(p == NULL || *p == '\0')
        return(0);

    if(p[0] >= 'A' && p[0] <= 'Z' && p[1] >= 'A' && p[1] <= 'Z')
    {
        if(p[2] >= '0' && p[2] <= '9')
            return(1);
        if(p[2] == '_')
        {
            if(p[3] >= '0' && p[3] <= '9')
                return(2);
            if(p[3] >= 'A' && p[3] <= 'Z' && p[4] >= 'A' && p[4] <= 'Z')
            {
                if(p[5] >= 'A' && p[5] <= 'Z' && p[6] >= 'A' && p[6] <= 'Z' &&
                   p[7] >= '0' && p[7] <= '9')
                    return(4);
                if(p[5] >= '0' && p[5] <= '9')
                    return(6);
            }
        }
        if(p[2] >= 'A' && p[2] <= 'Z' && p[3] >= 'A' && p[3] <= 'Z')
        {
            if(p[4] >= 'A' && p[4] <= 'Z' && p[5] >= 'A' && p[5] <= 'Z' &&
               p[6] >= '0' && p[6] <= '9')
                return(8);
            if(p[4] >= '0' && p[4] <= '9')
            {
                if(p[5] >= '0' && p[5] <= '9' && p[6] == 'S' &&
                   p[7] >= '0' && p[7] <= '9')
                    return(7);
                return(3);
            }

            if(p[4] >= 'A' && p[4] <= 'Z' && p[5] >= '0' && p[6] <= '9')
                return(5);
        }
    }
    return(0);
}

/**********************************************************/
static bool IsValidAccessPrefix(const char* acc, char** accpref)
{
    Int4 i = IsNewAccessFormat(acc);
    if(i == 0 || accpref == NULL)
        return false;

    if(i > 2 && i < 9)
        return true;

    char** b = accpref;
    for (; *b != NULL; b++)
    {
        if (StringNCmp(acc, *b, StringLen(*b)) == 0)
            break;
    }

    return (*b != NULL);
}

/**********************************************************/
static bool fta_if_master_wgs_accession(const char* acnum, Int4 accformat)
{
    const char* p;

    if(accformat == 3)
        p = acnum + 4;
    else if(accformat == 8)
        p = acnum + 6;
    else if(accformat == 4)
        p = acnum + 7;
    else
        return false;

    if(p[0] >= '0' && p[0] <= '9' && p[1] >= '0' && p[1] <= '9')
    {
        for(p += 2; *p == '0';)
            p++;
        if(*p == '\0')
            return true;
        return false;
    }
    return false;
}


static bool s_IsVDBWGSScaffold(const CTempString& accession)
{
    // 4+2+S+[6,7,8]
    if (accession.length() < 13 ||
        accession.length() > 15 ||
        accession[6] != 'S') {
        return false;
    }

    // check that the first 4 chars are letters
    if (any_of(begin(accession), 
              begin(accession)+4, 
              [](const char c){ return !isalpha(c); })) {
        return false;
    }

    // check that the next 2 chars are letters
    if (!isdigit(accession[4]) ||
        !isdigit(accession[5])) {
        return false;
    }

    // The characters after 'S' should all be digits 
    // with at least one non-zero digit

    // First check for digits
    if (any_of(begin(accession)+7, 
               end(accession), 
               [](const char c){ return !isdigit(c); })) {
        return false;
    }

    // Now check to see if at least one is not zero
    if (all_of(begin(accession)+7,
               end(accession),
               [](const char c) { return c == '0'; })) {
        return false;
    }

    return true;
}

static int s_RefineWGSType(const CTempString& accession, int initialType)
{
    if (initialType == -1) {
        return initialType;
    }
        // Identify as TSA or TLS
    if(accession[0] == 'G')                       /* TSA-WGS */
    {
        switch(initialType) 
        {
        case 0:
            return 4;
        case 1:
            return 5;
        case 3:
            return 6;
        default:
            return initialType;
        }
    }

    if (accession[0] == 'K' || accession[1] == 'T') { // TLS
        switch(initialType)
        {
        case 0:
            return 10;
        case 1:
            return 11;
        case 3:
            return 12;
        default:
            return initialType;
        }
    }

    if (initialType == 1) { // TSA again
        if (accession[0] == 'I') {
            return 8;
        }
        if (accession[0] == 'H') {
            return 9;
        }   
    }

    return initialType;
}

/**********************************************************/
/* Returns:  0 - if WGS project accession;
 *           1 - WGS contig accession;
 *           2 - WGS scaffold accession (2+6);
 *           3 - WGS master accession (XXXX00000000);
 *           4 - TSA-WGS project accession;
 *           5 - TSA-WGS contig accession
 *           6 - TSA-WGS master accession;
 *           7 - VDB WGS scaffold accession (4+2+S+[6,7,8]);
 *           8 - TSA-WGS contig DDBJ accession
 *           9 - TSA-WGS contig EMBL accession
 *          10 - TLS-WGS project accession;
 *          11 - TLS-WGS contig accession
 *          12 - TLS-WGS master accession;
 *          -1 - something else.
 */
int fta_if_wgs_acc(const CTempString& accession)
{

    if (accession.empty() ||
        NStr::IsBlank(accession)) {
        return -1;
    }

    const auto length = accession.length();

    if(length == 8 && 
       k_WgsScaffoldPrefix.find(accession.substr(0,2)) != k_WgsScaffoldPrefix.end() && 
       all_of(begin(accession)+2, end(accession), [](const char c) { return isdigit(c); })) {
        return 2;
    }

    if(length > 12 && length < 16 && accession[6] == 'S')
    {
        if (s_IsVDBWGSScaffold(accession)) {
            return 7;
        }
        return -1;
    }

    const char* p = accession.data();
    if(StringNCmp(p, "NZ_", 3) == 0) {
        p += 3;
    }
    size_t j = StringLen(p);
    if(j < 12 || j > 17) {
        return -1;
    }

    if(isdigit(p[4]))
    {
        if(all_of(p, p+4, [](const char c) { return isalpha(c); }) &&
           all_of(p+4, end(accession), [](const char c) { return isdigit(c); })) {

            int i = -1;
            if (any_of(p+6, end(accession), [](const char c) { return c != '0'; })) {
                i = 1; // WGS contig
            }
            else 
            if (p[4] == '0' && p[5] == '0') {
                i = 3; // WGS master 
            }
            else {
                i = 0; // WGS project
            }
            return s_RefineWGSType(p, i);
        }
        return -1;
    }


    // 6 letters + 2 digits
    if (all_of(p, p+6, [](const char c){ return isalpha(c); }) &&
        all_of(p+6, end(accession), [](const char c) { return isdigit(c); })) {

        if (any_of(p+8, end(accession), [](const char c) { return c != '0'; })) {
            return 1; // WGS contig
        }
            
        if (p[6] == '0' && p[7] == '0') {
            return 3; // WGS master 
        }
        return 0; // WGS project
    }

    return -1; // unknown
}

/**********************************************************/
bool IsSPROTAccession(const char* acc)
{
    const char **b;

    if(acc == NULL || acc[0] == '\0')
        return false;
    size_t len = StringLen(acc);
    if(len != 6 && len != 8 && len != 10)
        return false;
    if(len == 8)
    {
        for (b = sprot_accpref; *b != NULL; b++)
        {
            if (StringNCmp(*b, acc, 2) == 0)
                break;
        }

        return (*b != NULL);
    }

    if(acc[0] < 'A' || acc[0] > 'Z' || acc[1] < '0' || acc[1] > '9' ||
       ((acc[3] < '0' || acc[3] > '9') && (acc[3] < 'A' || acc[3] > 'Z')) ||
       ((acc[4] < '0' || acc[4] > '9') && (acc[4] < 'A' || acc[4] > 'Z')) ||
       acc[5] < '0' || acc[5] > '9')
        return false;

    if(acc[0] >= 'O' && acc[0] <= 'Q')
    {
        if((acc[2] < '0' || acc[2] > '9') && (acc[2] < 'A' || acc[2] > 'Z'))
            return false;
    }
    else if(acc[2] < 'A' || acc[2] > 'Z')
        return false;

    if(len == 6)
        return true;

    if(acc[0] >= 'O' && acc[0] <= 'Q')
        return false;

    if(acc[6] < 'A' || acc[6] > 'Z' || acc[9] < '0' || acc[9] > '9' ||
       ((acc[7] < 'A' || acc[7] > 'Z') && (acc[7] < '0' || acc[7] > '9')) ||
       ((acc[8] < 'A' || acc[8] > 'Z') && (acc[8] < '0' || acc[8] > '9')))
        return false;

    return true;
}


/*
static bool sCheckAccession(const list<string>& tokens, 
                            Parser::ESource source,
                            Parser::EMode mode,
                            const char* priacc, int skip)
{
    bool        badac;
    bool        res = true;
    bool        iswgs;
    Char        acnum[200];
    Int4        accformat;
    Int4        priformat;
    Int4        count;
    size_t        i;

    if(priacc == NULL || mode == Parser::EMode::Relaxed)
        return true;

    auto it = tokens.begin();
    if (skip) {
        advance(it, skip);
    }

    priformat = IsNewAccessFormat(priacc);
    if((priformat == 3 || priformat == 4 || priformat == 8) &&
       fta_if_master_wgs_accession(priacc, priformat) == false)
        iswgs = true;
    else
        iswgs = false;

    count = 0;
    for(; it != tokens.end(); ++it)
    {
        StringCpy(acnum, it->c_str());
        if(acnum[0] == '-' && acnum[1] == '\0')
            continue;

        if(skip == 2 && count == 0)
            accformat = priformat;
        else
            accformat = IsNewAccessFormat(acnum);

        size_t len = StringLen(acnum);
        if(acnum[len-1] == ';')
        {
            len--;
            acnum[len] = '\0';
        }
        badac = false;
        if(accformat == 1)
        {
            if(len != 8 && len != 10)
                badac = true;
            else
            {
                for(i = 2; i < 8 && badac == false; i++)
                    if(acnum[i] < '0' || acnum[i] > '9')
                        badac = true;
            }
        }
        else if(accformat == 2)
        {
            if(len != 9 && len != 12)
                badac = true;
            else
            {
                for(i = 3; i < len && badac == false; i++)
                    if(acnum[i] < '0' || acnum[i] > '9')
                        badac = true;
            }
        }
        else if(accformat == 3)
        {
            if(len < 12 || len > 14)
                badac = true;
            else
            {
                for(i = 4; i < len && badac == false; i++)
                    if(acnum[i] < '0' || acnum[i] > '9')
                        badac = true;
            }
        }
        else if(accformat == 8)
        {
            if(len < 15 || len > 17)
                badac = true;
            else
            {
                for(i = 6; i < len && !badac; i++)
                    if(acnum[i] < '0' || acnum[i] > '9')
                        badac = true;
            }
        }
        else if(accformat == 4)
        {
            if(len < 15 || len > 17)
                badac = true;
            else
            {
                for(i = 7; i < len && badac == false; i++)
                    if(acnum[i] < '0' || acnum[i] > '9')
                        badac = true;
            }
        }
        else if(accformat == 5)
        {
            if(len != 12)
                badac = true;
            else
            {
                for(i = 5; i < len && badac == false; i++)
                    if(acnum[i] < '0' || acnum[i] > '9')
                        badac = true;
            }
        }
        else if(accformat == 6)
        {
            if(len != 11 || acnum[0] != 'N' || acnum[1] != 'Z' ||
               acnum[2] != '_' || acnum[3] < 'A' || acnum[3] > 'Z' ||
               acnum[4] < 'A' || acnum[4] > 'Z')
                badac = true;
            else
            {
                for(i = 5; i < len && badac == false; i++)
                    if(acnum[i] < '0' || acnum[i] > '9')
                        badac = true;
            }
        }
        else if(accformat == 7)
        {
            if(len < 13 || len > 15)
                badac = true;
            else
            {
                for(i = 7; i < len && badac == false; i++)
                    if(acnum[i] < '0' || acnum[i] > '9')
                        badac = true;
            }
        }
        else if(accformat == 0)
        {
            if(len != 6 && len != 10)
                badac = true;
            else if(acnum[0] >= 'A' && acnum[0] <= 'Z')
            {
                if(source == Parser::ESource::SPROT)
                {
                    if(!IsSPROTAccession(acnum))
                        badac = true;
                }
                else if(len == 10)
                {
                    badac = true;
                }
                else
                {
                    for(i = 1; i < 6 && badac == false; i++)
                        if(acnum[i] < '0' || acnum[i] > '9')
                            badac = true;
                }
            }
            else
                badac = true;
        }
        else
            badac = true;

        if(badac)
        {
            ErrPostEx(SEV_ERROR, ERR_ACCESSION_BadAccessNum,
                      "Bad accession #, %s for this entry", acnum);
            res = false;
            count++;
            continue;
        }

        if(skip == 2 && count == 0 && !iswgs &&
           (accformat == 3 || accformat == 4 || accformat == 8))
        {
            ErrPostEx(SEV_REJECT, ERR_ACCESSION_WGSProjectAccIsPri,
                      "This record has a WGS 'project' accession as its primary accession number. WGS project-accessions are only expected to be used as secondary accession numbers.");
            res = false;
        }
        count++;
    }

    return(res);
}
*/

/**********************************************************
 *
 *   static bool CheckAccession(stoken, source, entryacc,
 *                                 skip):
 *
 *      A valid accession number should be an upper case
 *   letter (A-Z) followed by 5 digits, put "reject" message
 *   if not.
 *
 *                                              7-6-93
 *
 **********************************************************/
static bool CheckAccession(TokenStatBlkPtr stoken, 
                           Parser::ESource source,
                           Parser::EMode mode,
                           char* priacc, Int4 skip)
{
    TokenBlkPtr tbp;
    bool        badac;
    bool        res = true;
    bool        iswgs;
    Char        acnum[200];
    Int4        accformat;
    Int4        priformat;
    Int4        count;
    size_t        i;

    if(priacc == NULL || mode == Parser::EMode::Relaxed)
        return true;

    tbp = (skip == 0) ? stoken->list : stoken->list->next;
    priformat = IsNewAccessFormat(priacc);
    if((priformat == 3 || priformat == 4 || priformat == 8) &&
       fta_if_master_wgs_accession(priacc, priformat) == false)
        iswgs = true;
    else
        iswgs = false;

    count = 0;
    for(; tbp != NULL; tbp = tbp->next)
    {
        StringCpy(acnum, tbp->str);
        if(acnum[0] == '-' && acnum[1] == '\0')
            continue;

        if(skip == 2 && count == 0)
            accformat = priformat;
        else
            accformat = IsNewAccessFormat(acnum);

        size_t len = StringLen(acnum);
        if(acnum[len-1] == ';')
        {
            len--;
            acnum[len] = '\0';
        }
        badac = false;
        if(accformat == 1)
        {
            if(len != 8 && len != 10)
                badac = true;
            else
            {
                for(i = 2; i < 8 && badac == false; i++)
                    if(acnum[i] < '0' || acnum[i] > '9')
                        badac = true;
            }
        }
        else if(accformat == 2)
        {
            if(len != 9 && len != 12)
                badac = true;
            else
            {
                for(i = 3; i < len && badac == false; i++)
                    if(acnum[i] < '0' || acnum[i] > '9')
                        badac = true;
            }
        }
        else if(accformat == 3)
        {
            if(len < 12 || len > 14)
                badac = true;
            else
            {
                for(i = 4; i < len && badac == false; i++)
                    if(acnum[i] < '0' || acnum[i] > '9')
                        badac = true;
            }
        }
        else if(accformat == 8)
        {
            if(len < 15 || len > 17)
                badac = true;
            else
            {
                for(i = 6; i < len && !badac; i++)
                    if(acnum[i] < '0' || acnum[i] > '9')
                        badac = true;
            }
        }
        else if(accformat == 4)
        {
            if(len < 15 || len > 17)
                badac = true;
            else
            {
                for(i = 7; i < len && badac == false; i++)
                    if(acnum[i] < '0' || acnum[i] > '9')
                        badac = true;
            }
        }
        else if(accformat == 5)
        {
            if(len != 12)
                badac = true;
            else
            {
                for(i = 5; i < len && badac == false; i++)
                    if(acnum[i] < '0' || acnum[i] > '9')
                        badac = true;
            }
        }
        else if(accformat == 6)
        {
            if(len != 11 || acnum[0] != 'N' || acnum[1] != 'Z' ||
               acnum[2] != '_' || acnum[3] < 'A' || acnum[3] > 'Z' ||
               acnum[4] < 'A' || acnum[4] > 'Z')
                badac = true;
            else
            {
                for(i = 5; i < len && badac == false; i++)
                    if(acnum[i] < '0' || acnum[i] > '9')
                        badac = true;
            }
        }
        else if(accformat == 7)
        {
            if(len < 13 || len > 15)
                badac = true;
            else
            {
                for(i = 7; i < len && badac == false; i++)
                    if(acnum[i] < '0' || acnum[i] > '9')
                        badac = true;
            }
        }
        else if(accformat == 0)
        {
            if(len != 6 && len != 10)
                badac = true;
            else if(acnum[0] >= 'A' && acnum[0] <= 'Z')
            {
                if(source == Parser::ESource::SPROT)
                {
                    if(!IsSPROTAccession(acnum))
                        badac = true;
                }
                else if(len == 10)
                {
                    badac = true;
                }
                else
                {
                    for(i = 1; i < 6 && badac == false; i++)
                        if(acnum[i] < '0' || acnum[i] > '9')
                            badac = true;
                }
            }
            else
                badac = true;
        }
        else
            badac = true;

        if(badac)
        {
            ErrPostEx(SEV_ERROR, ERR_ACCESSION_BadAccessNum,
                      "Bad accession #, %s for this entry", acnum);
            res = false;
            count++;
            continue;
        }

        if(skip == 2 && count == 0 && !iswgs &&
           (accformat == 3 || accformat == 4 || accformat == 8))
        {
            ErrPostEx(SEV_REJECT, ERR_ACCESSION_WGSProjectAccIsPri,
                      "This record has a WGS 'project' accession as its primary accession number. WGS project-accessions are only expected to be used as secondary accession numbers.");
            res = false;
        }
        count++;
    }

    return(res);
}

/**********************************************************/
static bool IsPatentedAccPrefix(const Parser& parseInfo, const char* acc)
{
    if(acc[2] == '\0')
    {
        if((StringCmp(acc, "AR") == 0 || StringCmp(acc, "DZ") == 0 ||
            StringCmp(acc, "EA") == 0 || StringCmp(acc, "GC") == 0 ||
            StringCmp(acc, "GP") == 0 || StringCmp(acc, "GV") == 0 ||
            StringCmp(acc, "GX") == 0 || StringCmp(acc, "GY") == 0 ||
            StringCmp(acc, "GZ") == 0 || StringCmp(acc, "HJ") == 0 ||
            StringCmp(acc, "HK") == 0 || StringCmp(acc, "HL") == 0 ||
            StringCmp(acc, "KH") == 0 || StringCmp(acc, "MI") == 0 ||
            StringCmp(acc, "MM") == 0 || StringCmp(acc, "MO") == 0 ||
            StringCmp(acc, "MV") == 0 || StringCmp(acc, "MX") == 0 ||
            StringCmp(acc, "MY") == 0) &&
           (parseInfo.all == true || parseInfo.source == Parser::ESource::NCBI))
            return true;
        if((StringNCmp(acc, "AX", 2) == 0 || StringNCmp(acc, "CQ", 2) == 0 ||
            StringNCmp(acc, "CS", 2) == 0 || StringNCmp(acc, "FB", 2) == 0 ||
            StringNCmp(acc, "HA", 2) == 0 || StringNCmp(acc, "HB", 2) == 0 ||
            StringNCmp(acc, "HC", 2) == 0 || StringNCmp(acc, "HD", 2) == 0 ||
            StringNCmp(acc, "HH", 2) == 0 || StringNCmp(acc, "GM", 2) == 0 ||
            StringNCmp(acc, "GN", 2) == 0 || StringNCmp(acc, "JA", 2) == 0 ||
            StringNCmp(acc, "JB", 2) == 0 || StringNCmp(acc, "JC", 2) == 0 ||
            StringNCmp(acc, "JD", 2) == 0 || StringNCmp(acc, "JE", 2) == 0 ||
            StringNCmp(acc, "HI", 2) == 0 || StringNCmp(acc, "LP", 2) == 0 ||
            StringNCmp(acc, "LQ", 2) == 0 || StringNCmp(acc, "MP", 2) == 0 ||
            StringNCmp(acc, "MQ", 2) == 0 || StringNCmp(acc, "MR", 2) == 0 ||
            StringNCmp(acc, "MS", 2) == 0) &&
           (parseInfo.all == true || parseInfo.source == Parser::ESource::EMBL))
           return true;
        if ((StringNCmp(acc, "BD", 2) == 0 || StringNCmp(acc, "DD", 2) == 0 ||
            StringNCmp(acc, "DI", 2) == 0 || StringNCmp(acc, "DJ", 2) == 0 ||
            StringNCmp(acc, "DL", 2) == 0 || StringNCmp(acc, "DM", 2) == 0 ||
            StringNCmp(acc, "FU", 2) == 0 || StringNCmp(acc, "FV", 2) == 0 ||
            StringNCmp(acc, "FW", 2) == 0 || StringNCmp(acc, "FZ", 2) == 0 ||
            StringNCmp(acc, "GB", 2) == 0 || StringNCmp(acc, "HV", 2) == 0 ||
            StringNCmp(acc, "HW", 2) == 0 || StringNCmp(acc, "HZ", 2) == 0 ||
            StringNCmp(acc, "LF", 2) == 0 || StringNCmp(acc, "LG", 2) == 0 ||
            StringNCmp(acc, "LV", 2) == 0 || StringNCmp(acc, "LX", 2) == 0 ||
            StringNCmp(acc, "LY", 2) == 0 || StringNCmp(acc, "LZ", 2) == 0 ||
            StringNCmp(acc, "MA", 2) == 0 || StringNCmp(acc, "MB", 2) == 0 ||
            StringNCmp(acc, "MC", 2) == 0 || StringNCmp(acc, "MD", 2) == 0 ||
            StringNCmp(acc, "ME", 2) == 0 || StringNCmp(acc, "OF", 2) == 0 ||
            StringNCmp(acc, "OG", 2) == 0 || StringNCmp(acc, "OI", 2) == 0 ||
            StringNCmp(acc, "OJ", 2) == 0 || StringNCmp(acc, "PA", 2) == 0) &&
           (parseInfo.all == true || parseInfo.source == Parser::ESource::DDBJ))
           return true;

        return false;
    }

    if(acc[1] == '\0' && (*acc == 'I' || *acc == 'A' || *acc == 'E'))
    {
        if(parseInfo.all == true ||
           (*acc == 'I' && parseInfo.source == Parser::ESource::NCBI) ||
           (*acc == 'A' && parseInfo.source == Parser::ESource::EMBL) ||
           (*acc == 'E' && parseInfo.source == Parser::ESource::DDBJ))
           return true;
    }
    return false;
}

/**********************************************************/
static bool IsTPAAccPrefix(const Parser& parseInfo, const char* acc)
{
    if(acc == NULL)
        return(false);

    size_t i = StringLen(acc);
    if(i != 2 && i != 4)
        return(false);

    if(i == 4)
    {
        if(acc[0] == 'D' &&
           (parseInfo.all == true || parseInfo.source == Parser::ESource::NCBI))
            return(true);
        if(acc[0] == 'E' &&
           (parseInfo.all == true || parseInfo.source == Parser::ESource::DDBJ))
            return(true);
        return(false);
    }

    if(fta_StringMatch(ncbi_tpa_accpref, acc) > -1 &&
       (parseInfo.all == true || parseInfo.source == Parser::ESource::NCBI))
        return(true);
    if(fta_StringMatch(ddbj_tpa_accpref, acc) > -1 &&
       (parseInfo.all == true || parseInfo.source == Parser::ESource::DDBJ))
        return(true);
    return(false);
}

/**********************************************************/
static bool IsWGSAccPrefix(const Parser& parseInfo, const char* acc)
{
    if(acc == NULL || StringLen(acc) != 2)
        return(false);

    if(fta_StringMatch(ncbi_wgs_accpref, acc) > -1 &&
       (parseInfo.all == true || parseInfo.source == Parser::ESource::NCBI))
        return(true);
    if(fta_StringMatch(ddbj_wgs_accpref, acc) > -1 &&
       (parseInfo.all == true || parseInfo.source == Parser::ESource::DDBJ))
        return(true);
    return(false);
}

/**********************************************************/
static void IsTSAAccPrefix(const Parser& parseInfo, const char* acc, IndexblkPtr ibp)
{
    if(acc == NULL || *acc == '\0')
        return;

    if(parseInfo.source == Parser::ESource::EMBL)
    {
        ibp->tsa_allowed = true;
        return;
    }

    if(acc[0] == 'U' && acc[1] == '\0' &&
       (parseInfo.all == true || parseInfo.source == Parser::ESource::NCBI))
    {
        ibp->tsa_allowed = true;
        return;
    }

    if(StringLen(acc) != 2 && StringLen(acc) != 4)
        return;

    if(parseInfo.all == true || parseInfo.source == Parser::ESource::NCBI)
    {
        if((StringLen(acc) == 2 &&
            (StringCmp(acc, "EZ") == 0 || StringCmp(acc, "HP") == 0 ||
             StringCmp(acc, "JI") == 0 || StringCmp(acc, "JL") == 0 ||
             StringCmp(acc, "JO") == 0 || StringCmp(acc, "JP") == 0 ||
             StringCmp(acc, "JR") == 0 || StringCmp(acc, "JT") == 0 ||
             StringCmp(acc, "JU") == 0 || StringCmp(acc, "JV") == 0 ||
             StringCmp(acc, "JW") == 0 || StringCmp(acc, "KA") == 0)) ||
           fta_if_wgs_acc(ibp->acnum) == 5)
        {
            ibp->is_tsa = true;
            ibp->tsa_allowed = true;
        }
        if(fta_StringMatch(acc_tsa_allowed, acc) > -1)
            ibp->tsa_allowed = true;
    }

    if(parseInfo.all == true || parseInfo.source == Parser::ESource::DDBJ)
    {
        if(StringNCmp(acc, "FX", 2) == 0 || StringNCmp(acc, "LA", 2) == 0 ||
           StringNCmp(acc, "LE", 2) == 0 || StringNCmp(acc, "LH", 2) == 0 ||
           StringNCmp(acc, "LI", 2) == 0 || StringNCmp(acc, "LJ", 2) == 0 ||
           fta_if_wgs_acc(ibp->acnum) == 8)
        {
            ibp->is_tsa = true;
            ibp->tsa_allowed = true;
        }
    }

    if(parseInfo.all == true || parseInfo.source == Parser::ESource::EMBL)
    {
        if(fta_if_wgs_acc(ibp->acnum) == 9)
        {
            ibp->is_tsa = true;
            ibp->tsa_allowed = true;
        }
    }
}

/**********************************************************/
static void IsTLSAccPrefix(const Parser& parseInfo, const char* acc, IndexblkPtr ibp)
{
    if(acc == NULL || *acc == '\0' || StringLen(acc) != 4)
        return;

    if(parseInfo.all == true || parseInfo.source == Parser::ESource::NCBI ||
       parseInfo.source == Parser::ESource::DDBJ)
        if(fta_if_wgs_acc(ibp->acnum) == 11)
            ibp->is_tls = true;
}
/*
static bool sIsAccPrefixChar(char c)  {
    return (c >= 'A'  && c <= 'Z');
}
*/
/**********************************************************
 *
 *   bool GetAccession(pp, str, entry, skip):
 *
 *      Only record the first line of the first accession
 *   number.
 *      PIR format, accession number does not follow
 *   the rule.
 *
 *                                              3-4-93
 *
 **********************************************************/
/*
bool GetAccession(const Parser& parseInfo, const CTempString& str, IndexblkPtr entry, int skip)
{
    string accession;
    list<string> tokens;
    bool get = true;

    if((skip != 2 && parseInfo.source == Parser::ESource::Flybase) ||
       parserInfo.source == Parser::ESource::USPTO)
        return true;

    NStr::Split(str, " ;", tokens, NStr::fSplit_Tokenize);


    if (skip != 2)
    {
        get = ParseAccessionRange(tokens, skip);
        if (get)
            get = sCheckAccession(tokens, parseInfo.source, parseInfo.mode, entry->acnum, skip);
        if (!get) 
            entry->drop = 1;

        if (tokens.size()>skip && skip<2) { // Not sure about the logic
            auto it = skip ? next(tokens.begin(), skip) : tokens.begin();
            move(it, tokens.end(), entry->secondary_accessions.end());
        } 
        return get;
    }

    // skip == 2
    entry->is_tpa = false;
    if(tokens.size() < 2)
    {
        if (parseInfo.mode != Parser::EMode::Relaxed) {
            ErrPostEx(SEV_ERROR, ERR_ACCESSION_NoAccessNum,
                    "No accession # for this entry, about line %ld",
                    (long int) entry->linenum);
            entry->drop = 1;
        }
        return false;
    }
    

    accession = *next(tokens.begin());
    sDelNonDigitTail(accession);

    StringCpy(entry->acnum, accession.c_str());

    if (parseInfo.format != Parser::EFormat::XML) {
        string temp = accession;
        if (parseInfo.accver && entry->vernum > 0) {
            temp += "." + NStr::NumericToString(entry->vernum); 
        }
        if (temp.empty()) {
            if (entry->locusname[0] != '\0') {
                temp = entry->locusname;
            }
            else {
                temp = "???";
            } 
        }
        FtaInstallPrefix(PREFIX_ACCESSION, temp.c_str(), NULL);
    }

    if (parseInfo.source == Parser::ESource::Flybase) 
    {
        return true;
    }

    if (accession.size() < 2) {
        ErrPostEx(SEV_ERROR, ERR_ACCESSION_BadAccessNum,
                  "Wrong accession [%s] for this entry.", accession.c_str());
        entry->drop = 1;
        return false;
    }

    if (sIsAccPrefixChar(accession[0]) && sIsAccPrefixChar(accession[1])) {
        if (parseInfo.accpref && !IsValidAccessPrefix(accession.c_str(), parseInfo.accpref)) {
            get = false;
        }

        if (sIsAccPrefixChar(accession[2]) && sIsAccPrefixChar(accession[3])) {
            if (sIsAccPrefixChar(accession[4])) {
                accession = accession.substr(0,5);
            }
            else {
                accession = accession.substr(0,4);
            }
        }
        else if (accession[2] == '_') {
            accession = accession.substr(0,3);
        }
        else {
            accession = accession.substr(0,2);
        }
    }
    else {
        if (parseInfo.acprefix && !StringChr(parseInfo.acprefix, accession[0])) {
            get = false;
        }
        accession = accession.substr(0,1);
    }

    if (get) {
        if (tokens.size() > 2) {
            get = ParseAccessionRange(tokens,2);
            if (get) {
                get = sCheckAccession(tokens, parseInfo.source, parseInfo.mode, entry->acnum, 2);
            }
        }
    }
    else {
        string sourceName = sourceNames.at(parseInfo.source);
        ErrPostEx(SEV_ERROR, ERR_ACCESSION_BadAccessNum,
                  "Wrong accession # prefix [%s] for this source: %s",
                  accession.c_str(), sourceName.c_str());
    }

    entry->secondary_accessions.clear(); // Is this necessary?
    move(next(tokens.begin(),2), tokens.end(), entry->secondary_accessions.begin());

    if (!entry->is_pat) {
        entry->is_pat = IsPatentedAccPrefix(parseInfo, accession.c_str());
    }
    entry->is_tpa = IsTPAAccPrefix(parseInfo, accession.c_str());
    entry->is_wgs = IsWGSAccPrefix(parseInfo, accession.c_str());
    IsTSAAccPrefix(parseInfo, accession.c_str(), entry);
    IsTLSAccPrefix(parseInfo, accession.c_str(), entry);

    auto i = IsNewAccessFormat(entry->acnum);
    if(i == 3 || i == 8)
    {
        entry->is_wgs = true;
        entry->wgs_and_gi |= 02;
    }
    else if(i == 5)
    {
        char* p = entry->acnum;
        if(parseInfo.source != Parser::ESource::DDBJ || *p != 'A' || StringLen(p) != 12 ||
           StringCmp(p + 5, "0000000") != 0)
        {
            string sourceName = sourceNames.at(parseInfo.source);
            ErrPostEx(SEV_ERROR, ERR_ACCESSION_BadAccessNum,
                      "Wrong accession \"%s\" for this source: %s",
                      p, sourceName.c_str());
            get = false;
        }
        entry->is_mga = true;
    }

    if(!get)
        entry->drop = 1;

    return get;
}
*/


bool GetAccession(ParserPtr pp, char* str, IndexblkPtr entry, Int4 skip)
{
    Char            acc[200];
    Char            temp[400];
    char*         line;
    char*         p;
    TokenStatBlkPtr stoken;
    TokenBlkPtr     tbp;
    TokenBlkPtr     ttbp;
    bool            get = true;
    Int4            i;

    if((skip != 2 && pp->source == Parser::ESource::Flybase) ||
       pp->source == Parser::ESource::USPTO)
        return true;

    line = StringSave(str);
    for(p = line; *p != '\0'; p++)
        if(*p == ';')
            *p = ' ';
    stoken = TokenString(line, ' ');

    if(skip != 2)
    {
        get = ParseAccessionRange(stoken, skip);
        if(get)
            get = CheckAccession(stoken, pp->source, pp->mode, entry->acnum, skip);
        if(!get)
            entry->drop = 1;

        if(skip == 0)
        {
            tbp = stoken->list;
            stoken->list = NULL;
        }
        else if(skip == 1 && stoken->list != NULL)
        {
            tbp = stoken->list->next;
            stoken->list->next = NULL;
        }
        else
            tbp = NULL;
        if(tbp != NULL)
        {
            if(entry->secaccs == NULL)
                entry->secaccs = tbp;
            else
            {
                for(ttbp = entry->secaccs; ttbp->next != NULL;)
                    ttbp = ttbp->next;
                ttbp->next = tbp;
            }
        }

        FreeTokenstatblk(stoken);
        MemFree(line);
        return(get);
    }

    entry->is_tpa = false;
    acc[0] = '\0';
    if(stoken->num < 2)
    {
        if (pp->mode != Parser::EMode::Relaxed) {
            ErrPostEx(SEV_ERROR, ERR_ACCESSION_NoAccessNum,
                    "No accession # for this entry, about line %ld",
                    (long int) entry->linenum);
            entry->drop = 1;
        }
        FreeTokenstatblk(stoken);
        MemFree(line);
        return false;
    }

    StringCpy(acc, stoken->list->next->str);    /* get first accession */
    
    if (pp->mode != Parser::EMode::Relaxed) {
        DelNoneDigitTail(acc);
    }

    StringCpy(entry->acnum, acc);

    if(pp->format != Parser::EFormat::XML)
    {
        if(pp->accver && entry->vernum > 0)
            sprintf(temp, "%s.%d", acc, entry->vernum);
        else
            StringCpy(temp, acc);

        if(*temp == '\0')
        {
            if(entry->locusname[0] != '\0')
                StringCpy(temp, entry->locusname);
            else
                StringCpy(temp, "???");
        }
        FtaInstallPrefix(PREFIX_ACCESSION, temp, NULL);
    }

    if(pp->source == Parser::ESource::Flybase)
    {
        FreeTokenstatblk(stoken);
        MemFree(line);
        return true;
    }

    if((StringLen(acc) < 2) &&
        pp->mode != Parser::EMode::Relaxed)
    {
        ErrPostEx(SEV_ERROR, ERR_ACCESSION_BadAccessNum,
                  "Wrong accession [%s] for this entry.", acc);
        FreeTokenstatblk(stoken);
        entry->drop = 1;
        MemFree(line);
        return false;
    }

    if (pp->mode != Parser::EMode::Relaxed) {
        if(acc[0] >= 'A' && acc[0] <= 'Z' && acc[1] >= 'A' && acc[1] <= 'Z')
        {
            if(IsValidAccessPrefix(acc, pp->accpref) == false && pp->accpref != NULL)
                get = false;
            if(acc[2] >= 'A' && acc[2] <= 'Z' && acc[3] >= 'A' && acc[3] <= 'Z')
            {
                if(acc[4] >= 'A' && acc[4] <= 'Z') {
                    acc[5] = '\0';
                }
                else {
                    acc[4] = '\0';
                }
            }
            else if(acc[2] == '_') {
                acc[3] = '\0';
            }
            else {
                acc[2] = '\0';
            }
        }
        else
        {
            /* Processing of accession numbers in old format
            */
            /* check valid prefix accession number
            */
            if(pp->acprefix != NULL && StringChr(pp->acprefix, *acc) == NULL)
                get = false;
            acc[1] = '\0';
        }
    }

    if(get)
    {
        if (stoken->num > 2)
            get = ParseAccessionRange(stoken, 2);
        if (get) {
            get = CheckAccession(stoken, pp->source, pp->mode, entry->acnum, 2);
        }
    }
    else
    {
        string sourceName = sourceNames.at(pp->source);
        ErrPostEx(SEV_ERROR, ERR_ACCESSION_BadAccessNum,
                  "Wrong accession # prefix [%s] for this source: %s",
                  acc, sourceName.c_str());
    }

    entry->secaccs = stoken->list->next->next;
    stoken->list->next->next = NULL;

    FreeTokenstatblk(stoken);

    if(!entry->is_pat)
        entry->is_pat = IsPatentedAccPrefix(*pp, acc);
    entry->is_tpa = IsTPAAccPrefix(*pp, acc);
    entry->is_wgs = IsWGSAccPrefix(*pp, acc);
    IsTSAAccPrefix(*pp, acc, entry);
    IsTLSAccPrefix(*pp, acc, entry);

    i = IsNewAccessFormat(entry->acnum);
    if(i == 3 || i == 8)
    {
        entry->is_wgs = true;
        entry->wgs_and_gi |= 02;
    }
    else if(i == 5)
    {
        p = entry->acnum;
        if(pp->source != Parser::ESource::DDBJ || *p != 'A' || StringLen(p) != 12 ||
           StringCmp(p + 5, "0000000") != 0)
        {
            string sourceName = sourceNames.at(pp->source);
            ErrPostEx(SEV_ERROR, ERR_ACCESSION_BadAccessNum,
                      "Wrong accession \"%s\" for this source: %s",
                      p, sourceName.c_str());
            get = false;
        }
        entry->is_mga = true;
    }

    MemFree(line);

    if(!get)
        entry->drop = 1;

    return(get);
}

/**********************************************************/
void ResetParserStruct(ParserPtr pp)
{
    if(pp == NULL)
        return;

    if(pp->entrylist != NULL)
    {
        for(Int4 i = 0; i < pp->indx; i++)
            if(pp->entrylist[i] != NULL)
                FreeIndexblk(pp->entrylist[i]);

        MemFree(pp->entrylist);
        pp->entrylist = NULL;
    }

    pp->indx = 0;
    pp->curindx = 0;

    if(pp->pbp != NULL)
    {
        if(pp->pbp->ibp != NULL)
            delete pp->pbp->ibp;
        delete pp->pbp;
        pp->pbp = NULL;
    }


    if(pp->operon != NULL)
    {
        fta_operon_free(pp->operon);
        pp->operon = NULL;
    }
}

/**********************************************************
 *
 *   void FreeParser(pp):
 *
 *                                              3-5-93
 *
 **********************************************************/
/*
void FreeParser(ParserPtr pp)
{
    if(pp == NULL)
        return;

    ResetParserStruct(pp);

    if(pp->fpo != NULL)
        MemFree(pp->fpo);
    delete pp;
}
*/

/**********************************************************
 *
 *   void CloseFiles(pp):
 *
 *                                              3-4-93
 *
 **********************************************************/
void CloseFiles(ParserPtr pp)
{
    if(pp->qsfd != NULL)
    {
        fclose(pp->qsfd);
        pp->qsfd = NULL;
    }
}

/**********************************************************
 *
 *   void MsgSkipTitleFail(flatfile, finfo):
 *
 *                                              7-2-93
 *
 **********************************************************/
void MsgSkipTitleFail(const char *flatfile, FinfoBlkPtr finfo)
{
    ErrPostEx(SEV_ERROR, ERR_ENTRY_Begin,
              "No valid beginning of entry found in %s file", flatfile);

    MemFree(finfo);
}


/**********************************************************/
bool FindNextEntryBuf(bool end_of_file, FileBuf& fbuf, FinfoBlkPtr finfo, const char *str, Int2 len)
{
    bool done = end_of_file;
    while (!done && StringNCmp(finfo->str, str, len) != 0)
        done = XReadFileBuf(fbuf, finfo);

    return(done);
}


bool FindNextEntryBuf(bool end_of_file, FileBuf& fbuf, FinfoBlkPtr finfo, const CTempString& keyword)
{
    return FindNextEntryBuf(end_of_file, fbuf, finfo, keyword.data(), keyword.size());
}


/**********************************************************
 *
 *   bool FlatFileIndex(pp, (*fun)()):
 *
 *                                              10-6-93
 *
 **********************************************************/
bool FlatFileIndex(ParserPtr pp, void (*fun)(IndexblkPtr entry, char* offset, Int4 len))
{
    bool index;

    switch(pp->format)
    {
        case Parser::EFormat::GenBank:
            index = GenBankIndex(pp);
            break;
        case Parser::EFormat::EMBL:
            index = EmblIndex(pp, fun);
            break;
        case Parser::EFormat::SPROT:
            index = SprotIndex(pp, fun);
            break;
        case Parser::EFormat::PRF:
            index = PrfIndex(pp, fun);
            break;
        case Parser::EFormat::PIR:
            index = PirIndex(pp, fun);
            break;
        case Parser::EFormat::XML:
            index = XMLIndex(pp);
            break;
        default:
            index = false;
            fprintf(stderr, "Unknown flatfile format.\n");
            break;
    }
    return(index);
}

/**********************************************************/
const char **GetAccArray(Parser::ESource source)
{
    if(source == Parser::ESource::EMBL)
        return(embl_accpref);
    if(source == Parser::ESource::PIR)
        return(pir_accpref);
    if(source == Parser::ESource::PRF)
        return(prf_accpref);
    if(source == Parser::ESource::SPROT)
        return(sprot_accpref);
    if(source == Parser::ESource::LANL)
        return(lanl_accpref);
    if(source == Parser::ESource::DDBJ)
        return(ddbj_accpref);
    if(source == Parser::ESource::NCBI)
        return(ncbi_accpref);
    if(source == Parser::ESource::Refseq)
        return(refseq_accpref);
    return(NULL);
}

/**********************************************************/
CSeq_id::E_Choice GetNucAccOwner(const char* acc, bool is_tpa)
{
    Char    p[4];
    const char*q;

    if(acc == NULL)
        return objects::CSeq_id::e_not_set;

    size_t len = StringLen(acc);

    if(len > 8 && acc[2] == '_')
    {
        p[0] = acc[0];
        p[1] = acc[1];
        p[2] = acc[2];
        p[3] = '\0';
        if(MatchArrayString(refseq_accpref, p) > -1)
        {
            for(q = acc + 3; *q != '\0'; q++)
            {
                if(*q >= '0' && *q <= '9')
                    continue;
                break;
            }
            if(*q == '\0')
            {
                return(objects::CSeq_id::e_Other);
            }
        }
    }

    if(len != 6 && (len < 8 || len > 17))
        return objects::CSeq_id::e_not_set;

    if(len == 11)
    {
        if(acc[0] == 'N' && acc[1] == 'Z' && acc[2] == '_' &&
           acc[3] >= 'A' && acc[3] <= 'Z' && acc[4] >= 'A' && acc[4] <= 'Z')
        {
            for(q = acc + 5; *q != '\0'; q++)
                if(*q < '0' || *q > '9')
                    break;
            if(*q == '\0')
            {
                return objects::CSeq_id::e_Other;
            }
        }
        return objects::CSeq_id::e_not_set;
    }

    if(len == 6)
    {
        if(acc[0] < 'A' || acc[0] > 'Z' || acc[1] < '0' || acc[1] > '9' ||
           acc[2] < '0' || acc[2] > '9' || acc[3] < '0' || acc[3] > '9' ||
           acc[4] < '0' || acc[4] > '9' || acc[5] < '0' || acc[5] > '9')
           return objects::CSeq_id::e_not_set;

        if(StringChr(ParFlat_NCBI_AC, acc[0]) != NULL)
        {
            return objects::CSeq_id::e_Genbank;
        }
        if(StringChr(ParFlat_LANL_AC, acc[0]) != NULL)
        {
            return objects::CSeq_id::e_Genbank;
        }
        if(StringChr(ParFlat_DDBJ_AC, acc[0]) != NULL)
        {
            return objects::CSeq_id::e_Ddbj;
        }
        if(StringChr(ParFlat_EMBL_AC, acc[0]) != NULL)
        {
            if (!is_tpa)
                return objects::CSeq_id::e_Embl;
            return objects::CSeq_id::e_Tpe;
        }
        return objects::CSeq_id::e_not_set;
    }

    if(len > 11 && len < 16 && acc[2] != '_')
    {
        if(len == 12 && acc[0] == 'A' && acc[1] >= 'A' && acc[1] <= 'Z' &&
           acc[2] >= 'A' && acc[2] <= 'Z' && acc[3] >= 'A' &&
           acc[3] <= 'Z' && acc[4] >= 'A' && acc[4] <= 'Z' &&
           StringCmp(acc + 5, "0000000") == 0)
        {
            return(objects::CSeq_id::e_Ddbj);
        }

        if(((acc[0] < 'A' || acc[0] > 'S') &&
            (acc[0] < 'T' || acc[0] > 'W')) ||
           acc[1] < 'A' || acc[1] > 'Z' || acc[2] < 'A' || acc[2] > 'Z' ||
           acc[3] < 'A' || acc[3] > 'Z' || acc[4] < '0' || acc[4] > '9' ||
           acc[5] < '0' || acc[5] > '9' ||
           ((acc[6] < '0' || acc[6] > '9') && acc[6] != 'S') ||
           acc[7] < '0' || acc[7] > '9' ||
           acc[8] < '0' || acc[8] > '9' || acc[9] < '0' || acc[9] > '9' ||
           acc[10] < '0' || acc[10] > '9' || acc[11] < '0' || acc[11] > '9')
        {
            if(len != 15)
                return objects::CSeq_id::e_not_set;
        }

        if(len == 12 && acc[6] == 'S')
            return objects::CSeq_id::e_not_set;
        if(len == 15 && acc[6] != 'S' && acc[5] >= '0' && acc[5] <= '9')
            return objects::CSeq_id::e_not_set;

        if(len > 12 && (acc[12] < '0' || acc[12] > '9'))
            return objects::CSeq_id::e_not_set;
        if (len > 13 && (acc[13] < '0' || acc[13] > '9'))
            return objects::CSeq_id::e_not_set;
        if (len > 14 && (acc[14] < '0' || acc[14] > '9'))
            return objects::CSeq_id::e_not_set;

        if(acc[0] == 'A' || acc[0] == 'D' || acc[0] == 'G' ||
           (acc[0] > 'I' && acc[0] < 'O') || (acc[0] > 'O' && acc[0] < 'T') ||
           acc[0] == 'V' || acc[0] == 'W')
        {
            if(acc[0] == 'D')
                return objects::CSeq_id::e_Tpg;
            return objects::CSeq_id::e_Genbank;
        }
        if(acc[0] == 'B' || acc[0] == 'E' || acc[0] == 'I' || acc[0] == 'T')
        {
            if(acc[0] == 'E')
                return objects::CSeq_id::e_Tpd;
            return objects::CSeq_id::e_Ddbj;
        }
        if(acc[0] == 'C' || acc[0] == 'F' || acc[0] == 'O' || acc[0] == 'H' ||
           acc[0] == 'U')
        {
            if (!is_tpa)
                return objects::CSeq_id::e_Embl;
            return objects::CSeq_id::e_Tpe;
        }
        if(len != 15)
            return objects::CSeq_id::e_not_set;
    }

    if(len > 14 && len < 18)
    {
        if(acc[2] == '_')
        {
            if(acc[0] != 'N' || acc[1] != 'Z' || acc[2] != '_' ||
               ((acc[3] < 'A' || acc[3] > 'J') &&
                (acc[3] < 'L' || acc[3] > 'N')) ||
               acc[4] < 'A' || acc[4] > 'Z' || acc[5] < 'A' || acc[5] > 'Z' ||
               acc[6] < 'A' || acc[6] > 'Z' || acc[7] < '0' || acc[7] > '9' ||
               acc[8] < '0' || acc[8] > '9' || acc[9] < '0' || acc[9] > '9' ||
               acc[10] < '0' || acc[10] > '9' || acc[11] < '0' ||
               acc[11] > '9' || acc[12] < '0' || acc[12] > '9' ||
               acc[13] < '0' || acc[13] > '9' || acc[14] < '0' || acc[14] > '9')
               return objects::CSeq_id::e_not_set;

            if(len > 15 && (acc[15] < '0' || acc[15] > '9'))
                return objects::CSeq_id::e_not_set;
            if(len > 16 && (acc[16] < '0' || acc[16] > '9'))
                return objects::CSeq_id::e_not_set;
            return objects::CSeq_id::e_Other;
        }
        if((acc[0] != 'A' && acc[0] != 'B' && acc[0] != 'C') ||
           acc[1] < 'A' || acc[1] > 'Z' || acc[2] < 'A' || acc[2] > 'Z' ||
           acc[3] < 'A' || acc[3] > 'Z' || acc[4] < 'A' || acc[4] > 'Z' ||
           acc[5] < 'A' || acc[5] > 'Z' || acc[6] < '0' || acc[6] > '9' ||
           acc[7] < '0' || acc[7] > '9' || acc[8] < '0' || acc[8] > '9' ||
           acc[9] < '0' || acc[9] > '9' || acc[10] < '0' || acc[10] > '9' ||
           acc[11] < '0' || acc[11] > '9' || acc[12] < '0' || acc[12] > '9' ||
           acc[13] < '0' || acc[13] > '9' || acc[14] < '0' || acc[14] > '9')
            return objects::CSeq_id::e_not_set;

        if(len > 15 && (acc[15] < '0' || acc[15] > '9'))
            return objects::CSeq_id::e_not_set;
        if(len > 16 && (acc[16] < '0' || acc[16] > '9'))
            return objects::CSeq_id::e_not_set;

        if(acc[0] == 'A')
        {
            return objects::CSeq_id::e_Genbank;
        }
        if(acc[0] == 'B')
        {
            return objects::CSeq_id::e_Ddbj;
        }
        if(acc[0] == 'C')
        {
            return objects::CSeq_id::e_Embl;
        }
        return objects::CSeq_id::e_not_set;
    }

    q = acc + ((len == 8 || len == 10) ? 2 : 3);
    if(q[0] < '0' || q[0] > '9' || q[1] < '0' || q[1] > '9' ||
       q[2] < '0' || q[2] > '9' || q[3] < '0' || q[3] > '9' ||
       q[4] < '0' || q[4] > '9' || q[5] < '0' || q[5] > '9')
       return objects::CSeq_id::e_not_set;

    if(len == 9)
    {
        p[0] = acc[0];
        p[1] = acc[1];
        p[2] = acc[2];
        p[3] = '\0';
        if(MatchArrayString(refseq_accpref, p) > -1)
        {
            return objects::CSeq_id::e_Other;
        }
        return objects::CSeq_id::e_not_set;
    }

    if(acc[0] < 'A' || acc[0] > 'Z' || acc[1] < 'A' || acc[1] > 'Z')
        return objects::CSeq_id::e_not_set;

    p[0] = acc[0];
    p[1] = acc[1];
    p[2] = '\0';
    if(MatchArrayString(ncbi_accpref, p) > -1)
    {
        if(MatchArrayString(ncbi_tpa_accpref, p) > -1)
            return objects::CSeq_id::e_Tpg;
        return objects::CSeq_id::e_Genbank;
    }
    if(MatchArrayString(lanl_accpref, p) > -1)
    {
        return objects::CSeq_id::e_Genbank;
    }
    if(MatchArrayString(ddbj_accpref, p) > -1)
    {
        if(MatchArrayString(ddbj_tpa_accpref, p) > -1)
            return objects::CSeq_id::e_Tpd;
        return objects::CSeq_id::e_Ddbj;
    }
    if(MatchArrayString(embl_accpref, p) > -1)
    {
        if (!is_tpa)
            return objects::CSeq_id::e_Embl;
        return objects::CSeq_id::e_Tpe;
    }

    return objects::CSeq_id::e_not_set;
}



/**********************************************************/
Uint1 GetProtAccOwner(const Char* acc)
{
    const Char* q;
    Char    p[4];

    if(acc == NULL)
        return(0);

    size_t len = StringLen(acc);
    if(len == 9 || len == 12)
    {
        p[0] = acc[0];
        p[1] = acc[1];
        p[2] = acc[2];
        p[3] = '\0';
        if(MatchArrayString(refseq_prot_accpref, p) > -1)
        {
            for(q = &acc[3]; *q >= '0' && *q <= '9';)
                q++;
            if(*q == '\0')
                return objects::CSeq_id::e_Other;
        }
        return(0);
    }

    if(len != 8 && len != 10)
        return(0);

    if(acc[0] < 'A' || acc[0] > 'Z' || acc[1] < 'A' || acc[1] > 'Z' ||
       acc[2] < 'A' || acc[2] > 'Z' || acc[3] < '0' || acc[3] > '9' ||
       acc[4] < '0' || acc[4] > '9' || acc[5] < '0' || acc[5] > '9' ||
       acc[6] < '0' || acc[6] > '9' || acc[7] < '0' || acc[7] > '9')
    {
        if(len == 8)
            return(0);
        if(acc[8] < '0' || acc[8] > '9' || acc[9] < '0' || acc[9] > '9')
            return(0);
    }

    if(acc[0] == 'D' || acc[0] == 'H')
        return objects::CSeq_id::e_Tpg;
    if(acc[0] == 'F' || acc[0] == 'I')
        return objects::CSeq_id::e_Tpd;
    if(acc[0] == 'A' || acc[0] == 'E' || acc[0] == 'J' || acc[0] == 'K' ||
       (acc[0] > 'L' && acc[0] < 'S') || acc[0] == 'T' || acc[0] == 'U')
       return objects::CSeq_id::e_Genbank;
    if(acc[0] == 'B' || acc[0] == 'G' || acc[0] == 'L')
        return objects::CSeq_id::e_Ddbj;
    if(acc[0] == 'C' || acc[0] == 'S' || acc[0] == 'V')
        return objects::CSeq_id::e_Embl;

    return(0);
}

END_NCBI_SCOPE
