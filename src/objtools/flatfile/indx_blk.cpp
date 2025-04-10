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
 * File Name: indx_blk.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
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
#  undef THIS_FILE
#endif
#define THIS_FILE "indx_blk.cpp"


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

// clang-format off
static const char* XML_STRAND_array[] = {
    "   ", "single", "double", "mixed", nullptr
};

static const char* XML_TPG_array[] = {
    "   ", "Linear", "Circular", "Tandem", nullptr
};

static const char* ParFlat_NA_array_DDBJ[] = {
    "cDNA", nullptr
};

static const char* ParFlat_AA_array_DDBJ[] = {
    "PRT", nullptr
};

static const char* ParFlat_NA_array[] = {
    "    ", "NA", "DNA", "genomic DNA", "other DNA", "unassigned DNA", "RNA",
    "mRNA", "rRNA", "tRNA", "uRNA", "scRNA", "snRNA", "snoRNA", "pre-RNA",
    "pre-mRNA", "genomic RNA", "other RNA", "unassigned RNA", "cRNA",
    "viral cRNA", nullptr
};

static const char* ParFlat_DIV_array[] = {
    "   ", "PRI", "ROD", "MAM", "VRT", "INV", "PLN", "BCT", "RNA",
    "VRL", "PHG", "SYN", "UNA", "EST", "PAT", "STS", "ORG", "GSS",
    "HUM", "HTG", "CON", "HTC", "ENV", "TSA", nullptr
};

static const char* embl_accpref[] = {
    "AJ", "AL", "AM", "AN", "AX", "BN", "BX", "CQ", "CR", "CS", "CT", "CU",
    "FB", "FM", "FN", "FO", "FP", "FQ", "FR", "GM", "GN", "HA", "HB", "HC",
    "HD", "HE", "HF", "HG", "HH", "HI", "JA", "JB", "JC", "JD", "JE", "LK",
    "LL", "LM", "LN", "LO", "LP", "LQ", "LR", "LS", "LT", "MP", "MQ", "MR",
    "MS", "OA", "OB", "OC", "OD", "OE", "OU", "OV", "OW", "OX", "OY", "OZ",
    nullptr
};

static const char* lanl_accpref[] = {
    "AD", nullptr
};

static const char* sprot_accpref[] = {
    "DD", nullptr
};

static const char* ddbj_accpref[] = {
    "AB", "AG", "AK", "AP", "AT", "AU", "AV", "BA", "BB", "BD", "BJ", "BP",
    "BR", "BS", "BW", "BY", "CI", "CJ", "DA", "DB", "DC", "DD", "DE", "DF",
    "DG", "DH", "DI", "DJ", "DK", "DL", "DM", "FS", "FT", "FU", "FV", "FW",
    "FX", "FY", "FZ", "GA", "GB", "HT", "HU", "HV", "HW", "HX", "HY", "HZ",
    "LA", "LB", "LC", "LD", "LE", "LF", "LG", "LH", "LI", "LJ", "LU", "LV",
    "LX", "LY", "LZ", "MA", "MB", "MC", "MD", "ME", "OF", "OG", "OH", "OI",
    "OJ", "PA", "PB", "PC", "PD", "PE", "PF", "PG", "PH", "PI", "PJ", "PK",
    "PL", "PM", nullptr
};

static const char* ncbi_accpref[] = {
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
    "OO", "OP", "OQ", "OR", "OS", "OT", "PP", "PQ", "PR", "PS", "PT", "PU",
    "PV", nullptr
};

static const char* refseq_accpref[] = {
    "NC_", "NG_", "NM_", "NP_", "NR_", "NT_", "NW_", "XM_", "XP_", "XR_",
    "NZ_", nullptr
};

/*
static const char* refseq_prot_accpref[] = {
    "AP_", "NP_", "WP_", "XP_", "YP_", "ZP_", nullptr
};
*/

static const char* acc_tsa_allowed[] = {
    "AF", "AY", "DQ", "EF", "EU", "FJ", "GQ", "HQ", "JF", "JN", "JQ", "JX",
    "KC", "KF", "KJ", "KM", "KP", "KR", "KT", "KU", "KX", "KY", "MF", "MG",
    "MH", "MK", "MN", "MT", nullptr
};

static const char* ncbi_tpa_accpref[] = {
    "BK", "BL", "GJ", "GK", nullptr
};

static const char* ddbj_tpa_accpref[] = {
    "BR", "HT", "HU", nullptr
};

static const char* ncbi_wgs_accpref[] = {
    "GJ", "GK", nullptr
};

static const char* ddbj_wgs_accpref[] = {
    "HT", "HU", nullptr
};

static const set<string_view> k_WgsScaffoldPrefix = {
    "CH", "CT", "CU", "DF", "DG", "DS",
    "EM", "EN", "EP", "EQ", "FA", "FM",
    "GG", "GJ", "GK", "GL", "HT", "HU",
    "JH", "KB", "KD", "KE", "KI", "KK",
    "KL", "KN", "KQ", "KV", "KZ", "LD",
    "ML", "MU", "PS"
};

static const map<Parser::ESource, string> sourceNames = {
    { Parser::ESource::unknown, "unknown" },
    { Parser::ESource::EMBL, "EMBL" },
    { Parser::ESource::GenBank, "GENBANK" },
    { Parser::ESource::SPROT, "Swiss-Prot" },
    { Parser::ESource::NCBI, "NCBI" },
    { Parser::ESource::LANL, "GSDB" },
    { Parser::ESource::Flybase, "FlyBase" },
    { Parser::ESource::Refseq, "RefSeq" }
};

static const char* month_name[] = {
    "Ill", "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC", nullptr
};

static const char* ParFlat_RESIDUE_STR[] = {
    "bp", "bp.", "bp,", "AA", "AA.", "AA,", nullptr
};

static const char* ValidMolTypes[] = {
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
    nullptr
};
// clang-format on

// functions below are implemented in different source files
bool EmblIndex(ParserPtr pp, void (*fun)(IndexblkPtr entry, char* offset, Int4 len));
bool GenBankIndex(ParserPtr pp, void (*fun)(IndexblkPtr entry, char* offset, Int4 len));
bool SprotIndex(ParserPtr pp, void (*fun)(IndexblkPtr entry, char* offset, Int4 len));
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
static const char* GetResidue(TokenStatBlkPtr stoken)
{
    const char** b;
    Int2         i;

    auto ptr  = stoken->list.begin();
    auto sptr = next(ptr);
    for (i = 1; i < stoken->num; i++, ptr = sptr, sptr = next(ptr)) {
        for (b = ParFlat_RESIDUE_STR; *b; b++)
            if (NStr::CompareNocase(*b, *sptr) == 0)
                return ptr->c_str();
    }

    return nullptr;
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
static bool XReadFile(FILE* fp, FinfoBlk& finfo)
{
    bool end_of_file = false;

    StringCpy(finfo.str, "\n");
    while (! end_of_file && (finfo.str[0] == '\n')) {
        finfo.pos = (size_t)ftell(fp);
        if (! fgets(finfo.str, sizeof(finfo.str) - 1, fp))
            end_of_file = true;
        else
            ++finfo.line;
    }

    auto n = strlen(finfo.str);
    while (n) {
        n--;
        if (finfo.str[n] != '\n' && finfo.str[n] != '\r') {
            break;
        }
        finfo.str[n] = 0;
    }

    return (end_of_file);
}

/**********************************************************/
static Int2 FileGetsBuf(char* res, Int4 size, FileBuf& fbuf)
{
    const char* p = nullptr;
    char*       q;
    Int4        l;
    Int4        i;

    if (fbuf.current == nullptr || *fbuf.current == '\0')
        return (0);

    l = size - 1;
    for (p = fbuf.current, q = res, i = 0; i < l; i++, p++) {
        *q++ = *p;
        if (*p == '\n' || *p == '\r') {
            p++;
            break;
        }
    }

    *q           = '\0';
    fbuf.current = p;
    return (1);
}

/**********************************************************/
bool XReadFileBuf(FileBuf& fbuf, FinfoBlk& finfo)
{
    bool end_of_file = false;

    StringCpy(finfo.str, "\n");
    while (! end_of_file && (finfo.str[0] == '\n')) {
        finfo.pos = fbuf.get_offs();
        if (FileGetsBuf(finfo.str, sizeof(finfo.str) - 1, fbuf) == 0)
            end_of_file = true;
        else
            ++finfo.line;
    }

    return (end_of_file);
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
NCBI_UNUSED
bool SkipTitle(FILE* fp, FinfoBlk& finfo, const char* str, size_t len)
{
    bool end_of_file = XReadFile(fp, finfo);
    while (! end_of_file && ! StringEquN(finfo.str, str, len))
        end_of_file = XReadFile(fp, finfo);

    return (end_of_file);
}

NCBI_UNUSED
bool SkipTitle(FILE* fp, FinfoBlk& finfo, string_view keyword)
{
    return SkipTitle(fp, finfo, keyword.data(), keyword.size());
}

//  ----------------------------------------------------------------------------
bool SkipTitleBuf(FileBuf& fbuf, FinfoBlk& finfo, string_view keyword)
//  ----------------------------------------------------------------------------
{
    const char* p           = keyword.data();
    size_t      len         = keyword.size();
    bool        end_of_file = XReadFileBuf(fbuf, finfo);
    while (! end_of_file && ! StringEquN(finfo.str, p, len))
        end_of_file = XReadFileBuf(fbuf, finfo);

    return end_of_file;
}


/**********************************************************
 *
 *   static bool CheckLocus(locus):
 *
 *      Locus name only allow A-Z, 0-9, characters,
 *   reject if not.
 *
 **********************************************************/
static bool CheckLocus(const char* locus, Parser::ESource source)
{
    const char* p = locus;
    if (StringEquN(locus, "SEG_", 4) &&
        (source == Parser::ESource::NCBI || source == Parser::ESource::DDBJ))
        p += 4;
    for (; *p != '\0'; p++) {
        if ((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'Z') ||
            (*p == '.' && source == Parser::ESource::Flybase))
            continue;
        if (((*p >= 'a' && *p <= 'z') || *p == '_' || *p == '-' || *p == '(' ||
             *p == ')' || *p == '/') &&
            source == Parser::ESource::Refseq)
            continue;

        FtaErrPost(SEV_ERROR, ERR_LOCUS_BadLocusName, "Bad locusname, <{}> for this entry", locus);
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
static bool CheckLocusSP(const char* locus)
{
    const char* p;

    bool underscore = false;
    Int2 x;
    Int2 y;

    for (p = locus, x = y = 0; *p != '\0'; p++) {
        if ((*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'Z')) {
            if (! underscore)
                x++;
            else
                y++;
        } else if (*p == '_')
            underscore = true;
        else
            break;
    }

    if (*p != '\0' || x == 0 || y == 0) {
        FtaErrPost(SEV_ERROR, ERR_LOCUS_BadLocusName, "Bad locusname, <{}> for this entry", locus);
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
static bool CkDateFormat(string_view date)
{
    if (date[2] == '-' && date[6] == '-' &&
        isdigit(date[0]) && isdigit(date[1]) &&
        isdigit(date[7]) && isdigit(date[8]) &&
        isdigit(date[9]) && isdigit(date[10]) &&
        MatchArraySubString(month_name, date) >= 0)
        return true;

    return false;
}

/**********************************************************/
int CheckSTRAND(const string& str)
{
    static const vector<string_view> strandSpecs = {
        "   ", "ss-", "ds-", "ms-"
    };

    string compare(str);
    NStr::ToLower(compare);
    for (unsigned i = 0; i < strandSpecs.size(); ++i) {
        if (compare.starts_with(strandSpecs[i])) {
            return int(i);
        }
    }
    return -1;
}

/**********************************************************/
Int2 XMLCheckSTRAND(string_view str)
{
    return (StringMatchIcase(XML_STRAND_array, str));
}

/**********************************************************/
Int2 XMLCheckTPG(string_view str)
{
    Int2 i;

    i = StringMatchIcase(XML_TPG_array, str);
    if (i == 0)
        i = 1;
    return (i);
}

/**********************************************************/
int CheckTPG(const string& str)
{
    static const vector<string_view> topologies = {
        "         ", "linear   ", "circular ", "tandem   "
    };

    string compare(str);
    NStr::ToLower(compare);
    for (unsigned i = 0; i < topologies.size(); ++i) {
        if (compare.starts_with(topologies[i])) {
            return int(i);
        }
    }
    return -1;
}

/**********************************************************/
Int2 CheckNADDBJ(const char* str)
{
    return (fta_StringMatch(ParFlat_NA_array_DDBJ, str));
}

/**********************************************************/
Int2 CheckNA(const char* str)
{
    return (fta_StringMatch(ParFlat_NA_array, str));
}

/**********************************************************/
Int2 CheckDIV(const char* str)
{
    return (fta_StringMatch(ParFlat_DIV_array, str));
}

/**********************************************************/
bool CkLocusLinePos(char* offset, Parser::ESource source, LocusContPtr lcp, bool is_mga)
{
    Char  date[12];
    bool  ret = true;
    char* p;
    Int4  i;

    p = StringChr(offset, '\n');
    if (p)
        *p = '\0';

    if (is_mga == false && ! fta_StartsWith(offset + lcp->bp, "bp"sv) &&
        ! fta_StartsWith(offset + lcp->bp, "rc"sv) &&
        ! fta_StartsWith(offset + lcp->bp, "aa"sv)) {
        i = lcp->bp + 1;
        FtaErrPost(SEV_WARNING, ERR_FORMAT_LocusLinePosition, "bp/rc string unrecognized in column {}-{}: {}", i, i + 1, offset + lcp->bp);
        ret = false;
    }
    if (CheckSTRAND(offset + lcp->strand) == -1) {
        i = lcp->strand + 1;
        FtaErrPost(SEV_WARNING, ERR_FORMAT_LocusLinePosition, "Strand unrecognized in column {}-{} : {}", i, i + 2, offset + lcp->strand);
    }

    p = offset + lcp->molecule;
    if (is_mga) {
        if (! fta_StartsWithNocase(p, "mRNA"sv) && ! fta_StartsWith(p, "RNA"sv)) {
            FtaErrPost(SEV_REJECT, ERR_FORMAT_IllegalCAGEMoltype, "Illegal molecule type provided in CAGE record in LOCUS line: \"{}\". Must be \"mRNA\"or \"RNA\". Entry dropped.", p);
            ret = false;
        }
    } else if (StringMatchIcase(ParFlat_NA_array, p) < 0) {
        if (StringMatchIcase(ParFlat_AA_array_DDBJ, p) < 0) {
            i = lcp->molecule + 1;
            if (source != Parser::ESource::DDBJ ||
                StringMatchIcase(ParFlat_NA_array_DDBJ, p) < 0) {
                FtaErrPost(SEV_WARNING, ERR_FORMAT_LocusLinePosition, "Molecule unrecognized in column {}-{}: {}", i, i + 5, p);
                ret = false;
            }
        }
    }

    if (CheckTPG(offset + lcp->topology) == -1) {
        i = lcp->topology + 1;
        FtaErrPost(SEV_WARNING, ERR_FORMAT_LocusLinePosition, "Topology unrecognized in column {}-{}: {}", i, i + 7, offset + lcp->topology);
        ret = false;
    }
    if (CheckDIV(offset + lcp->div) == -1) {
        i = lcp->div + 1;
        FtaErrPost(SEV_WARNING, ERR_FORMAT_LocusLinePosition, "Division code unrecognized in column {}-{}: {}", i, i + 2, offset + lcp->div);
        ret = (source == Parser::ESource::LANL);
    }
    MemCpy(date, offset + lcp->date, 11);
    date[11] = '\0';
    if (fta_StartsWith(date, "NODATE"sv)) {
        FtaErrPost(SEV_WARNING, ERR_FORMAT_LocusLinePosition, "NODATE in LOCUS line will be replaced by current system date");
    } else if (! CkDateFormat(date)) {
        i = lcp->date + 1;
        FtaErrPost(SEV_WARNING, ERR_FORMAT_LocusLinePosition, "Date should be in column {}-{}, and format dd-mmm-yyyy: {}", i, i + 10, date);
        ret = false;
    }

    if (p)
        *p = '\n';
    return (ret);
}

/**********************************************************
 *
 *   CRef<CDate_std> GetUpdateDate(ptr, source):
 *
 *      Return NULL if ptr does not have dd-mmm-yyyy format
 *   or "NODATE"; otherwise, return Date-std pointer.
 *
 **********************************************************/
CRef<CDate_std> GetUpdateDate(const char* ptr, Parser::ESource source)
{

    if (fta_StartsWith(ptr, "NODATE"sv))
        return CRef<CDate_std>(new CDate_std(CTime(CTime::eCurrent)));

    if (ptr[11] != '\0' && ptr[11] != '\n' && ptr[11] != ' ' &&
        (source != Parser::ESource::SPROT || ptr[11] != ',')) {
        return {};
    }

    if (! CkDateFormat(ptr)) {
        FtaErrPost(SEV_ERROR, ERR_DATE_IllegalDate, "Invalid date: {}", ptr);
        return {};
    }

    return get_full_date(ptr, false, source);
}


/**********************************************************/
static bool fta_check_embl_moltype(char* str)
{
    const char** b;
    char*        p;
    char*        q;

    p = StringChr(str, ';');
    p = StringChr(p + 1, ';');
    p = StringChr(p + 1, ';');

    for (p++; *p == ' ';)
        p++;

    q  = StringChr(p, ';');
    *q = '\0';

    for (b = ValidMolTypes; *b; b++)
        if (StringEqu(p, *b))
            break;

    if (*b) {
        *q = ';';
        return true;
    }

    FtaErrPost(SEV_REJECT, ERR_FORMAT_InvalidIDlineMolType, "Invalid moltype value \"{}\" provided in ID line of EMBL record.", p);
    *q = ';';
    return false;
}

/*********************************************************
Indexblk constructor
**********************************************************/
Indexblk::Indexblk()
{
    acnum[0]      = 0;
    locusname[0]  = 0;
    division[0]   = 0;
    blocusname[0] = 0;
    wgssec[0]     = 0;
}

static bool isSpace(char c)
{
    return isspace(c);
}

static string_view::const_iterator
sFindNextSpace(string_view                 tempString,
               string_view::const_iterator current_it)
{
    return find_if(current_it, tempString.end(), isSpace);
}


static string_view::const_iterator
sFindNextNonSpace(string_view                 tempString,
                  string_view::const_iterator current_it)
{
    return find_if_not(current_it, tempString.end(), isSpace);
}


static void sSetLocusLineOffsets(string_view locusLine, LocusCont& offsets)
{
    offsets.bases    = -1;
    offsets.bp       = -1;
    offsets.strand   = -1;
    offsets.molecule = -1;
    offsets.topology = -1;
    offsets.div      = -1;
    offsets.date     = -1;

    if (locusLine.substr(0, 5) != "LOCUS"sv) {
        // throw an exception - invalid locus line
    }


    auto it = sFindNextNonSpace(locusLine, locusLine.begin() + 5);
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
    if (NStr::StringToNonNegativeInt(locusLine.substr(it - begin(locusLine), space_it - it)) == -1) {
        return;
    }

    offsets.bases = Int4(it - begin(locusLine));
    it            = sFindNextNonSpace(locusLine, space_it);
    offsets.bp    = Int4(it - begin(locusLine));

    it = sFindNextSpace(locusLine, it);
    it = sFindNextNonSpace(locusLine, it);

    // the next one might be a strand
    // or might be a molecule
    space_it       = sFindNextSpace(locusLine, it);
    offsets.strand = -1;
    if ((space_it - it) == 3) {
        auto currentSubstr = locusLine.substr(it - begin(locusLine), 3);
        if (currentSubstr == "ss-"sv ||
            currentSubstr == "ds-"sv ||
            currentSubstr == "ms-"sv) {
            offsets.strand = Int4(it - begin(locusLine));
            it             = sFindNextNonSpace(locusLine, space_it);
        }
        offsets.molecule = Int4(it - begin(locusLine));
    } else {
        offsets.molecule = Int4(it - begin(locusLine));
    }

    // topology
    it = sFindNextSpace(locusLine, it);
    it = sFindNextNonSpace(locusLine, it);
    if (it != locusLine.end()) {
        offsets.topology = Int4(it - begin(locusLine));
    }

    // find division
    it = sFindNextSpace(locusLine, it);
    it = sFindNextNonSpace(locusLine, it);
    if (it != locusLine.end()) {
        offsets.div = Int4(it - begin(locusLine));
    }

    // find date - date is optional
    it = sFindNextSpace(locusLine, it);
    it = sFindNextNonSpace(locusLine, it);
    if (it != locusLine.end()) {
        offsets.date = Int4(it - begin(locusLine));
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
IndexblkPtr InitialEntry(ParserPtr pp, FinfoBlk& finfo)
{
    Int2        i;
    Int2        j;
    const char* bases;
    IndexblkPtr entry;
    char*       p;

    entry = new Indexblk;

    entry->offset  = finfo.pos;
    entry->linenum = finfo.line;
    entry->ppp     = pp;
    entry->is_tsa  = false;
    entry->is_tls  = false;
    entry->is_pat  = false;
    entry->biodrop = false;

    auto stoken = TokenString(finfo.str, ' ');

    bool badlocus = false;
    if (stoken->num > 2) {
        p = finfo.str;
        if (pp->mode == Parser::EMode::Relaxed) {
            sSetLocusLineOffsets(p, entry->lc);
        } else {
            if (StringLen(p) > 78 && p[28] == ' ' && p[63] == ' ' && p[67] == ' ') {
                entry->lc.bases    = ParFlat_COL_BASES_NEW;
                entry->lc.bp       = ParFlat_COL_BP_NEW;
                entry->lc.strand   = ParFlat_COL_STRAND_NEW;
                entry->lc.molecule = ParFlat_COL_MOLECULE_NEW;
                entry->lc.topology = ParFlat_COL_TOPOLOGY_NEW;
                entry->lc.div      = ParFlat_COL_DIV_NEW;
                entry->lc.date     = ParFlat_COL_DATE_NEW;
            } else {
                entry->lc.bases    = ParFlat_COL_BASES;
                entry->lc.bp       = ParFlat_COL_BP;
                entry->lc.strand   = ParFlat_COL_STRAND;
                entry->lc.molecule = ParFlat_COL_MOLECULE;
                entry->lc.topology = ParFlat_COL_TOPOLOGY;
                entry->lc.div      = ParFlat_COL_DIV;
                entry->lc.date     = ParFlat_COL_DATE;
            }
        }

        auto ptr = stoken->list.begin();
        ++ptr;
        if (pp->format == Parser::EFormat::EMBL &&
            next(ptr) != stoken->list.end() && *next(ptr) == "SV"s) {
            for (i = 0, p = finfo.str; *p != '\0'; p++)
                if (*p == ';' && p[1] == ' ')
                    i++;

            entry->embl_new_ID = true;
            if (! ptr->empty() && ptr->back() == ';')
                ptr->pop_back();

            FtaInstallPrefix(PREFIX_LOCUS, *ptr);
            FtaInstallPrefix(PREFIX_ACCESSION, *ptr);

            if (i != 6 || (stoken->num != 10 && stoken->num != 11)) {
                FtaErrPost(SEV_REJECT, ERR_FORMAT_BadlyFormattedIDLine, "The number of fields in this EMBL record's new ID line does not fit requirements.");
                badlocus = true;
            } else if (fta_check_embl_moltype(finfo.str) == false)
                badlocus = true;
        }

        StringCpy(entry->locusname, ptr->c_str());
        StringCpy(entry->blocusname, entry->locusname);

        if (entry->embl_new_ID == false) {
            FtaInstallPrefix(PREFIX_LOCUS, entry->locusname);
            FtaInstallPrefix(PREFIX_ACCESSION, entry->locusname);
        }

        if (pp->mode != Parser::EMode::Relaxed && ! badlocus) {
            if (pp->format == Parser::EFormat::SPROT) {
                auto it = next(ptr);
                if (it == stoken->list.end() || it->empty() ||
                    (! fta_StartsWithNocase(it->c_str(), "preliminary"sv) &&
                     ! fta_StartsWithNocase(it->c_str(), "unreviewed"sv)))
                    badlocus = CheckLocusSP(entry->locusname);
                else
                    badlocus = false;
            } else
                badlocus = CheckLocus(entry->locusname, pp->source);
        }
    } else if (pp->mode != Parser::EMode::Relaxed) {
        badlocus = true;
        FtaErrPost(SEV_ERROR, ERR_LOCUS_NoLocusName, "No locus name for this entry");
    }

    if (badlocus) {
        p = StringChr(finfo.str, '\n');
        if (p)
            *p = '\0';
        FtaErrPost(SEV_ERROR, ERR_ENTRY_Skipped, "Entry skipped. LOCUS line = \"{}\".", finfo.str);
        if (p)
            *p = '\n';
        delete entry;
        return nullptr;
    }

    bases = GetResidue(stoken.get());
    if (bases)
        entry->bases = (size_t)atoi(bases);

    if (pp->format == Parser::EFormat::GenBank &&
        entry->lc.date > -1) {
        /* last token in the LOCUS line is date of the update's data
         */
        auto it = stoken->list.begin();
        for (i = 1; i < stoken->num; ++i)
            ++it;
        entry->date = GetUpdateDate(it->c_str(), pp->source);
    }

    if (pp->source == Parser::ESource::DDBJ || pp->source == Parser::ESource::EMBL) {
        j = stoken->num - ((pp->format == Parser::EFormat::GenBank) ? 2 : 3);
        auto it = stoken->list.begin();
        for (i = 1; i < j; ++i)
            ++it;

        if (pp->format == Parser::EFormat::EMBL) {
            if (fta_StartsWithNocase(it->c_str(), "TSA"sv))
                entry->is_tsa = true;
            else if (fta_StartsWithNocase(it->c_str(), "PAT"sv))
                entry->is_pat = true;
        }

        ++it;

        if (fta_StartsWithNocase(it->c_str(), "EST"sv))
            entry->EST = true;
        else if (fta_StartsWithNocase(it->c_str(), "STS"sv))
            entry->STS = true;
        else if (fta_StartsWithNocase(it->c_str(), "GSS"sv))
            entry->GSS = true;
        else if (fta_StartsWithNocase(it->c_str(), "HTC"sv))
            entry->HTC = true;
        else if (fta_StartsWithNocase(it->c_str(), "PAT"sv) &&
                 pp->source == Parser::ESource::EMBL)
            entry->is_pat = true;
    }

    return (entry);
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

    if (! str || *str == '\0')
        return;

    for (p = str; *str != '\0'; str++)
        if (*str >= '0' && *str <= '9')
            p = str + 1;

    *p = '\0';
}

void DelNonDigitTail(string& str)
{
    if (str.empty()) {
        return;
    }
    auto pos = str.find_last_of("0123456789");
    if (pos != string::npos) {
        str.resize(pos + 1);
    }
}

/**********************************************************
 *
 * Here X is an alpha character, N - numeric one.
 * Return values:
 *
 * 1 - XXN        (AB123456)
 * 2 - XX_N       (NZ_123456)
 * 3 - XXXXN      (AAAA01000001)
 * 4 - XX_XXXXN   (NZ_AAAA01000001)
 * 5 - XXXXXN     (AAAAA1234512)
 * 6 - XX_XXN     (NZ_AB123456)
 * 7 - XXXXNNSN   (AAAA01S000001 - scaffolds)
 * 8 - XXXXXXN    (AAAAAA010000001)
 * 9 - XXXXXXNNSN (AAAAAA01S0000001 - scaffolds)
 * 0 - all others
 *
 */

inline bool sIsUpperAlpha(char c)
{
    return (c >= 'A' && c <= 'Z');
}

Int4 IsNewAccessFormat(const Char* acnum)
{
    const Char* p = acnum;

    if (! p || *p == '\0')
        return 0;

    if (sIsUpperAlpha(p[0]) && sIsUpperAlpha(p[1])) {
        if (isdigit(p[2]))
            return 1;

        if (p[2] == '_') {
            if (isdigit(p[3])) {
                return 2;
            }
            if (sIsUpperAlpha(p[3]) && sIsUpperAlpha(p[4])) {
                if (sIsUpperAlpha(p[5]) && sIsUpperAlpha(p[6]) &&
                    isdigit(p[7]))
                    return 4;
                if (isdigit(p[5]))
                    return 6;
            }
            return 0;
        }

        if (sIsUpperAlpha(p[2]) && sIsUpperAlpha(p[3])) {
            if (sIsUpperAlpha(p[4]) && sIsUpperAlpha(p[5]) &&
                isdigit(p[6])) {
                if (isdigit(p[7]) && p[8] == 'S' &&
                    isdigit(p[9])) {
                    return 9;
                }
                return 8;
            }

            if (isdigit(p[4])) {
                if (isdigit(p[5]) && p[6] == 'S' &&
                    isdigit(p[7])) {
                    return 7;
                }
                return 3;
            }

            if (sIsUpperAlpha(p[4]) && isdigit(p[5]))
                return 5;
        }
    }
    return 0;
}


/**********************************************************/
static bool IsValidAccessPrefix(const char* acc, const char** accpref)
{
    Int4 i = IsNewAccessFormat(acc);
    if (i == 0 || ! accpref)
        return false;

    if (2 < i && i < 10)
        return true;

    const char** b = accpref;
    for (; *b; b++) {
        if (StringEquN(acc, *b, StringLen(*b)))
            return true;
    }

    return false;
}

/**********************************************************/
static bool fta_if_master_wgs_accession(const char* acnum, Int4 accformat)
{
    const char* p;

    if (accformat == 3)
        p = acnum + 4;
    else if (accformat == 8)
        p = acnum + 6;
    else if (accformat == 4)
        p = acnum + 7;
    else
        return false;

    if (p[0] >= '0' && p[0] <= '9' && p[1] >= '0' && p[1] <= '9') {
        for (p += 2; *p == '0';)
            p++;
        if (*p == '\0')
            return true;
        return false;
    }
    return false;
}


static bool s_IsVDBWGSScaffold(string_view accession)
{
    // 4+2+S+[6,7,8]
    if (accession.length() < 13 ||
        accession.length() > 15 ||
        accession[6] != 'S') {
        return false;
    }

    // check that the first 4 chars are letters
    if (any_of(begin(accession),
               begin(accession) + 4,
               [](const char c) { return ! isalpha(c); })) {
        return false;
    }

    // check that the next 2 chars are letters
    if (! isdigit(accession[4]) ||
        ! isdigit(accession[5])) {
        return false;
    }

    // The characters after 'S' should all be digits
    // with at least one non-zero digit

    // First check for digits
    if (any_of(begin(accession) + 7,
               end(accession),
               [](const char c) { return ! isdigit(c); })) {
        return false;
    }

    // Now check to see if at least one is not zero
    if (all_of(begin(accession) + 7,
               end(accession),
               [](const char c) { return c == '0'; })) {
        return false;
    }

    return true;
}

static int s_RefineWGSType(string_view accession, int initialType)
{
    if (initialType == -1) {
        return initialType;
    }
    // Identify as TSA or TLS
    if (accession[0] == 'G') /* TSA-WGS */
    {
        switch (initialType) {
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

    if (accession[0] == 'K' || accession[0] == 'T') { // TLS
        switch (initialType) {
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
int fta_if_wgs_acc(string_view accession)
{
    if (accession.empty() || NStr::IsBlank(accession)) {
        return -1;
    }

    auto length = accession.length();

    if (length == 8 &&
        k_WgsScaffoldPrefix.find(accession.substr(0, 2)) != k_WgsScaffoldPrefix.end() &&
        all_of(begin(accession) + 2, end(accession), [](const char c) { return isdigit(c); })) {
        return 2;
    }

    if (length > 12 && length < 16 && accession[6] == 'S') {
        if (s_IsVDBWGSScaffold(accession)) {
            return 7;
        }
        return -1;
    }

    if (accession.substr(0, 3) == "NZ_"sv) {
        accession = accession.substr(3);
    }
    length = accession.length();
    if (length < 12 || length > 17) {
        return -1;
    }

    if (isdigit(accession[4])) {
        if (all_of(begin(accession), begin(accession) + 4, [](const char c) { return isalpha(c); }) &&
            all_of(begin(accession) + 4, end(accession), [](const char c) { return isdigit(c); })) {

            int i = -1;
            if (any_of(begin(accession) + 6, end(accession), [](const char c) { return c != '0'; })) {
                i = 1; // WGS contig
            } else if (accession[4] == '0' && accession[5] == '0') {
                i = 3; // WGS master
            } else {
                i = 0; // WGS project
            }
            return s_RefineWGSType(accession, i);
        }
        return -1;
    }

    // 6 letters + 2 digits
    if (all_of(begin(accession), begin(accession) + 6, [](const char c) { return isalpha(c); }) &&
        all_of(begin(accession) + 6, end(accession), [](const char c) { return isdigit(c); })) {

        if (any_of(begin(accession) + 8, end(accession), [](const char c) { return c != '0'; })) {
            return 1; // WGS contig
        }

        if (accession[6] == '0' && accession[7] == '0') {
            return 3; // WGS master
        }
        return 0; // WGS project
    }

    return -1; // unknown
}

/**********************************************************/
bool IsSPROTAccession(const char* acc)
{
    const char** b;

    if (! acc || acc[0] == '\0')
        return false;
    size_t len = StringLen(acc);
    if (len != 6 && len != 8 && len != 10)
        return false;
    if (len == 8) {
        for (b = sprot_accpref; *b; b++) {
            if (StringEquN(*b, acc, 2))
                break;
        }

        return (*b != nullptr);
    }

    if (acc[0] < 'A' || acc[0] > 'Z' || acc[1] < '0' || acc[1] > '9' ||
        ((acc[3] < '0' || acc[3] > '9') && (acc[3] < 'A' || acc[3] > 'Z')) ||
        ((acc[4] < '0' || acc[4] > '9') && (acc[4] < 'A' || acc[4] > 'Z')) ||
        acc[5] < '0' || acc[5] > '9')
        return false;

    if (acc[0] >= 'O' && acc[0] <= 'Q') {
        if ((acc[2] < '0' || acc[2] > '9') && (acc[2] < 'A' || acc[2] > 'Z'))
            return false;
    } else if (acc[2] < 'A' || acc[2] > 'Z')
        return false;

    if (len == 6)
        return true;

    if (acc[0] >= 'O' && acc[0] <= 'Q')
        return false;

    if (acc[6] < 'A' || acc[6] > 'Z' || acc[9] < '0' || acc[9] > '9' ||
        ((acc[7] < 'A' || acc[7] > 'Z') && (acc[7] < '0' || acc[7] > '9')) ||
        ((acc[8] < 'A' || acc[8] > 'Z') && (acc[8] < '0' || acc[8] > '9')))
        return false;

    return true;
}


static inline bool sNotAllDigits(const char* first, const char* last)
{
    return any_of(first, last, [](char c) { return ! isdigit(c); });
}

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
static bool CheckAccession(
    TokenStatBlkPtr tokens,
    Parser::ESource source,
    Parser::EMode   mode,
    const char*     priacc,
    unsigned        skip)
{
    bool badac;
    bool res = true;
    bool iswgs;
    Char acnum[200];
    Int4 accformat;
    Int4 priformat;
    Int4 count;

    if (! priacc || mode == Parser::EMode::Relaxed)
        return true;

    auto tbp = tokens->list.begin();
    if (skip > 0)
        ++tbp; // advance(it, skip)
    priformat = IsNewAccessFormat(priacc);
    if ((priformat == 3 || priformat == 4 || priformat == 8) &&
        fta_if_master_wgs_accession(priacc, priformat) == false)
        iswgs = true;
    else
        iswgs = false;

    count = 0;
    for (; tbp != tokens->list.end(); ++tbp) {
        StringCpy(acnum, tbp->c_str());
        if (acnum[0] == '-' && acnum[1] == '\0')
            continue;

        if (skip == 2 && count == 0)
            accformat = priformat;
        else
            accformat = IsNewAccessFormat(acnum);

        size_t len = StringLen(acnum);
        if (acnum[len - 1] == ';') {
            len--;
            acnum[len] = '\0';
        }
        badac = false;
        if (accformat == 1) {
            badac = (len != 8 && len != 10) || sNotAllDigits(acnum + 2, acnum + 8);
        } else if (accformat == 2) {
            badac = (len != 9 && len != 12) || sNotAllDigits(acnum + 3, acnum + len);
        } else if (accformat == 3) {
            badac = (len < 12 || len > 14) || sNotAllDigits(acnum + 4, acnum + len);
        } else if (accformat == 8) {
            badac = (len < 15 || len > 17) || sNotAllDigits(acnum + 6, acnum + len);
        } else if (accformat == 4) {
            badac = (len < 15 || len > 17) || sNotAllDigits(acnum + 7, acnum + len);
        } else if (accformat == 5) {
            badac = (len != 12) || sNotAllDigits(acnum + 5, acnum + len);
        } else if (accformat == 6) {
            badac = (len != 11 || acnum[0] != 'N' || acnum[1] != 'Z' ||
                     acnum[2] != '_' || acnum[3] < 'A' || acnum[3] > 'Z' ||
                     acnum[4] < 'A' || acnum[4] > 'Z') ||
                    sNotAllDigits(acnum + 5, acnum + len);
        } else if (accformat == 7) {
            badac = (len < 13 || len > 15) || sNotAllDigits(acnum + 7, acnum + len);
        } else if (accformat == 9) {
            badac = (len < 16 || len > 17) || sNotAllDigits(acnum + 9, acnum + len);
        } else if (accformat == 0) {
            if (len != 6 && len != 10)
                badac = true;
            else if (sIsUpperAlpha(acnum[0])) {
                if (source == Parser::ESource::SPROT) {
                    if (! IsSPROTAccession(acnum))
                        badac = true;
                } else {
                    badac = (len == 10) || sNotAllDigits(acnum + 1, acnum + 6);
                }
            } else
                badac = true;
        } else
            badac = true;

        if (badac) {
            FtaErrPost(SEV_ERROR, ERR_ACCESSION_BadAccessNum, "Bad accession #, {} for this entry", acnum);
            res = false;
            count++;
            continue;
        }

        if (skip == 2 && count == 0 && ! iswgs &&
            (accformat == 3 || accformat == 4 || accformat == 8)) {
            FtaErrPost(SEV_REJECT, ERR_ACCESSION_WGSProjectAccIsPri, "This record has a WGS 'project' accession as its primary accession number. WGS project-accessions are only expected to be used as secondary accession numbers.");
            res = false;
        }
        count++;
    }

    return (res);
}

/**********************************************************/
static bool IsPatentedAccPrefix(const Parser& parseInfo, string_view acc)
{
    if (acc.length() == 2) {
        if (parseInfo.all || parseInfo.source == Parser::ESource::NCBI) {
            if (((acc == "AR"sv) || (acc == "DZ"sv) ||
                 (acc == "EA"sv) || (acc == "GC"sv) ||
                 (acc == "GP"sv) || (acc == "GV"sv) ||
                 (acc == "GX"sv) || (acc == "GY"sv) ||
                 (acc == "GZ"sv) || (acc == "HJ"sv) ||
                 (acc == "HK"sv) || (acc == "HL"sv) ||
                 (acc == "KH"sv) || (acc == "MI"sv) ||
                 (acc == "MM"sv) || (acc == "MO"sv) ||
                 (acc == "MV"sv) || (acc == "MX"sv) ||
                 (acc == "MY"sv) || (acc == "OO"sv) ||
                 (acc == "OS"sv) || (acc == "OT"sv) ||
                 (acc == "PR"sv) || (acc == "PT"sv) ||
                 (acc == "PU"sv)))
                return true;
        }
        if (parseInfo.all || parseInfo.source == Parser::ESource::EMBL) {
            if (((acc == "AX"sv) || (acc == "CQ"sv) ||
                 (acc == "CS"sv) || (acc == "FB"sv) ||
                 (acc == "HA"sv) || (acc == "HB"sv) ||
                 (acc == "HC"sv) || (acc == "HD"sv) ||
                 (acc == "HH"sv) || (acc == "GM"sv) ||
                 (acc == "GN"sv) || (acc == "JA"sv) ||
                 (acc == "JB"sv) || (acc == "JC"sv) ||
                 (acc == "JD"sv) || (acc == "JE"sv) ||
                 (acc == "HI"sv) || (acc == "LP"sv) ||
                 (acc == "LQ"sv) || (acc == "MP"sv) ||
                 (acc == "MQ"sv) || (acc == "MR"sv) ||
                 (acc == "MS"sv)))
                return true;
        }
        if (parseInfo.all || parseInfo.source == Parser::ESource::DDBJ) {
            if (((acc == "BD"sv) || (acc == "DD"sv) ||
                 (acc == "DI"sv) || (acc == "DJ"sv) ||
                 (acc == "DL"sv) || (acc == "DM"sv) ||
                 (acc == "FU"sv) || (acc == "FV"sv) ||
                 (acc == "FW"sv) || (acc == "FZ"sv) ||
                 (acc == "GB"sv) || (acc == "HV"sv) ||
                 (acc == "HW"sv) || (acc == "HZ"sv) ||
                 (acc == "LF"sv) || (acc == "LG"sv) ||
                 (acc == "LV"sv) || (acc == "LX"sv) ||
                 (acc == "LY"sv) || (acc == "LZ"sv) ||
                 (acc == "MA"sv) || (acc == "MB"sv) ||
                 (acc == "MC"sv) || (acc == "MD"sv) ||
                 (acc == "ME"sv) || (acc == "OF"sv) ||
                 (acc == "OG"sv) || (acc == "OI"sv) ||
                 (acc == "OJ"sv) || (acc == "PA"sv) ||
                 (acc == "PB"sv) || (acc == "PC"sv) ||
                 (acc == "PD"sv) || (acc == "PE"sv) ||
                 (acc == "PF"sv) || (acc == "PG"sv) ||
                 (acc == "PH"sv) || (acc == "PI"sv) ||
                 (acc == "PJ"sv) || (acc == "PK"sv) ||
                 (acc == "PL"sv) || (acc == "PM"sv)))
                return true;
        }

        return false;
    }

    if (acc.length() == 1 && (acc[0] == 'I' || acc[0] == 'A' || acc[0] == 'E')) {
        if (parseInfo.all ||
            (acc[0] == 'I' && parseInfo.source == Parser::ESource::NCBI) ||
            (acc[0] == 'A' && parseInfo.source == Parser::ESource::EMBL) ||
            (acc[0] == 'E' && parseInfo.source == Parser::ESource::DDBJ))
            return true;
    }
    return false;
}

/**********************************************************/
static bool IsTPAAccPrefix(const Parser& parseInfo, string_view acc)
{
    if (acc.empty())
        return (false);

    size_t i = acc.length();
    if (i != 2 && i != 4)
        return (false);

    if (i == 4) {
        if (acc[0] == 'D' &&
            (parseInfo.all || parseInfo.source == Parser::ESource::NCBI))
            return (true);
        if ((acc[0] == 'E' || acc[0] == 'Y') &&
            (parseInfo.all || parseInfo.source == Parser::ESource::DDBJ))
            return (true);
        return (false);
    }

    if (fta_StringMatch(ncbi_tpa_accpref, acc) >= 0 &&
        (parseInfo.all || parseInfo.source == Parser::ESource::NCBI))
        return (true);
    if (fta_StringMatch(ddbj_tpa_accpref, acc) >= 0 &&
        (parseInfo.all || parseInfo.source == Parser::ESource::DDBJ))
        return (true);
    return (false);
}

/**********************************************************/
static bool IsWGSAccPrefix(const Parser& parseInfo, string_view acc)
{
    if (acc.empty() || acc.length() != 2)
        return (false);

    if (fta_StringMatch(ncbi_wgs_accpref, acc) >= 0 &&
        (parseInfo.all || parseInfo.source == Parser::ESource::NCBI))
        return (true);
    if (fta_StringMatch(ddbj_wgs_accpref, acc) >= 0 &&
        (parseInfo.all || parseInfo.source == Parser::ESource::DDBJ))
        return (true);
    return (false);
}

/**********************************************************/
static void IsTSAAccPrefix(const Parser& parseInfo, string_view acc, IndexblkPtr ibp)
{
    if (acc.empty())
        return;

    if (parseInfo.source == Parser::ESource::EMBL ||
        parseInfo.source == Parser::ESource::DDBJ) {
        ibp->tsa_allowed = true;
        return;
    }

    if (acc == "U"sv &&
        (parseInfo.all || parseInfo.source == Parser::ESource::NCBI)) {
        ibp->tsa_allowed = true;
        return;
    }

    if (acc.length() != 2 && acc.length() != 4)
        return;

    if (parseInfo.all || parseInfo.source == Parser::ESource::NCBI) {
        if ((acc.length() == 2 &&
             ((acc == "EZ"sv) || (acc == "HP"sv) ||
              (acc == "JI"sv) || (acc == "JL"sv) ||
              (acc == "JO"sv) || (acc == "JP"sv) ||
              (acc == "JR"sv) || (acc == "JT"sv) ||
              (acc == "JU"sv) || (acc == "JV"sv) ||
              (acc == "JW"sv) || (acc == "KA"sv))) ||
            fta_if_wgs_acc(ibp->acnum) == 5) {
            ibp->is_tsa      = true;
            ibp->tsa_allowed = true;
        }
        if (fta_StringMatch(acc_tsa_allowed, acc) >= 0)
            ibp->tsa_allowed = true;
    }

    if (parseInfo.all || parseInfo.source == Parser::ESource::DDBJ) {
        if (acc.starts_with("FX"sv) || acc.starts_with("LA"sv) ||
            acc.starts_with("LE"sv) || acc.starts_with("LH"sv) ||
            acc.starts_with("LI"sv) || acc.starts_with("LJ"sv) ||
            fta_if_wgs_acc(ibp->acnum) == 8) {
            ibp->is_tsa      = true;
            ibp->tsa_allowed = true;
        }
    }

    if (parseInfo.all || parseInfo.source == Parser::ESource::EMBL) {
        if (fta_if_wgs_acc(ibp->acnum) == 9) {
            ibp->is_tsa      = true;
            ibp->tsa_allowed = true;
        }
    }
}

/**********************************************************/
static void IsTLSAccPrefix(const Parser& parseInfo, string_view acc, IndexblkPtr ibp)
{
    if (acc.empty() || acc.length() != 4)
        return;

    if (parseInfo.all || parseInfo.source == Parser::ESource::NCBI ||
        parseInfo.source == Parser::ESource::DDBJ)
        if (fta_if_wgs_acc(ibp->acnum) == 11)
            ibp->is_tls = true;
}

static inline bool sIsAccPrefixChar(char c)
{
    return (c >= 'A' && c <= 'Z');
}

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
bool GetAccession(const Parser* pp, string_view str, IndexblkPtr entry, unsigned skip)
{
    Char   acc[200]; // string
    bool   get = true;

    if ((skip != 2 && pp->source == Parser::ESource::Flybase) ||
        pp->source == Parser::ESource::USPTO)
        return true;

    string line(str);
    auto   tokens = TokenString(line.c_str(), ';');

    if (skip != 2) {
        get = ParseAccessionRange(tokens.get(), skip);
        if (get)
            get = CheckAccession(tokens.get(), pp->source, pp->mode, entry->acnum, skip);
        if (! get)
            entry->drop = true;

        if (skip == 1 && ! tokens->list.empty()) {
            tokens->list.pop_front();
            skip = 0;
        }
        if (skip == 0 && ! tokens->list.empty()) {
            auto tail = entry->secaccs.before_begin();
            for (; next(tail) != entry->secaccs.end();)
                ++tail;
            entry->secaccs.splice_after(tail, tokens->list);
        }

        return (get);
    }

    entry->is_tpa = false;
    acc[0]        = '\0';
    if (tokens->num < 2) {
        if (pp->mode != Parser::EMode::Relaxed) {
            FtaErrPost(SEV_ERROR, ERR_ACCESSION_NoAccessNum, "No accession # for this entry, about line {}", (long int)entry->linenum);
            entry->drop = true;
        }
        return false;
    }

    StringCpy(acc, next(tokens->list.begin())->c_str()); /* get first accession */

    if (pp->mode != Parser::EMode::Relaxed) {
        DelNoneDigitTail(acc);
    }

    StringCpy(entry->acnum, acc);

    if (pp->format != Parser::EFormat::XML) {
        string temp = acc;
        if (pp->accver && entry->vernum > 0) {
            temp += '.';
            temp += to_string(entry->vernum);
        }

        if (temp.empty()) {
            if (entry->locusname[0] != '\0')
                temp = entry->locusname;
            else
                temp = "???";
        }
        FtaInstallPrefix(PREFIX_ACCESSION, temp);
    }

    if (pp->source == Parser::ESource::Flybase) {
        return true;
    }

    if ((StringLen(acc) < 2) &&
        pp->mode != Parser::EMode::Relaxed) {
        FtaErrPost(SEV_ERROR, ERR_ACCESSION_BadAccessNum, "Wrong accession [{}] for this entry.", acc);
        entry->drop = true;
        return false;
    }

    if (pp->mode != Parser::EMode::Relaxed) {
        if (sIsAccPrefixChar(acc[0]) && sIsAccPrefixChar(acc[1])) {
            if (pp->accpref && ! IsValidAccessPrefix(acc, pp->accpref))
                get = false;
            if (sIsAccPrefixChar(acc[2]) && sIsAccPrefixChar(acc[3])) {
                if (sIsAccPrefixChar(acc[4])) {
                    acc[5] = '\0';
                } else {
                    acc[4] = '\0';
                }
            } else if (acc[2] == '_') {
                acc[3] = '\0';
            } else {
                acc[2] = '\0';
            }
        } else {
            /* Processing of accession numbers in old format */
            /* check valid prefix accession number */
            if (pp->acprefix && ! StringChr(pp->acprefix, acc[0]))
                get = false;
            acc[1] = '\0';
        }
    }

    if (get) {
        if (tokens->num > 2)
            get = ParseAccessionRange(tokens.get(), 2);
        if (get) {
            get = CheckAccession(tokens.get(), pp->source, pp->mode, entry->acnum, 2);
        }
    } else {
        string sourceName = sourceNames.at(pp->source);
        FtaErrPost(SEV_ERROR, ERR_ACCESSION_BadAccessNum, "Wrong accession # prefix [{}] for this source: {}", acc, sourceName);
    }

    tokens->list.pop_front();
    tokens->list.pop_front();
    entry->secaccs = std::move(tokens->list);
    tokens.reset();

    if (! entry->is_pat)
        entry->is_pat = IsPatentedAccPrefix(*pp, acc);
    entry->is_tpa = IsTPAAccPrefix(*pp, acc);
    entry->is_wgs = IsWGSAccPrefix(*pp, acc);
    IsTSAAccPrefix(*pp, acc, entry);
    IsTLSAccPrefix(*pp, acc, entry);

    auto i = IsNewAccessFormat(entry->acnum);
    if (i == 3 || i == 8) {
        entry->is_wgs = true;
        entry->wgs_and_gi |= 02;
    } else if (i == 5) {
        const char* p = entry->acnum;
        if (pp->source != Parser::ESource::DDBJ || *p != 'A' || StringLen(p) != 12 ||
            ! StringEqu(p + 5, "0000000")) {
            string sourceName = sourceNames.at(pp->source);
            FtaErrPost(SEV_ERROR, ERR_ACCESSION_BadAccessNum, "Wrong accession \"{}\" for this source: {}", p, sourceName);
            get = false;
        }
        entry->is_mga = true;
    }

    if (! get)
        entry->drop = true;

    return get;
}

/**********************************************************/
void ResetParserStruct(ParserPtr pp)
{
    if (! pp)
        return;

    if (! pp->entrylist.empty()) {
        for (Indexblk* ibp : pp->entrylist)
            if (ibp)
                delete ibp;

        pp->entrylist.clear();
    }

    pp->indx    = 0;
    pp->curindx = 0;

    if (pp->pbp) {
        if (pp->pbp->ibp)
            delete pp->pbp->ibp;
        delete pp->pbp;
        pp->pbp = nullptr;
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
    if (! pp)
        return;

    ResetParserStruct(pp);

    if (pp->fpo)
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
    if (pp->qsfd) {
        fclose(pp->qsfd);
        pp->qsfd = nullptr;
    }
}

/**********************************************************
 *
 *   void MsgSkipTitleFail(flatfile, finfo):
 *
 *                                              7-2-93
 *
 **********************************************************/
void MsgSkipTitleFail(const char* flatfile, FinfoBlk& finfo)
{
    FtaErrPost(SEV_ERROR, ERR_ENTRY_Begin, "No valid beginning of entry found in {} file", flatfile);

    // delete finfo;
}


bool FindNextEntryBuf(bool end_of_file, FileBuf& fbuf, FinfoBlk& finfo, string_view keyword)
{
    bool done = end_of_file;
    while (! done && ! fta_StartsWith(finfo.str, keyword))
        done = XReadFileBuf(fbuf, finfo);

    return (done);
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

    switch (pp->format) {
    case Parser::EFormat::GenBank:
        index = GenBankIndex(pp, fun);
        break;
    case Parser::EFormat::EMBL:
        index = EmblIndex(pp, fun);
        break;
    case Parser::EFormat::SPROT:
        index = SprotIndex(pp, fun);
        break;
    case Parser::EFormat::XML:
        index = XMLIndex(pp);
        break;
    default:
        index = false;
        fprintf(stderr, "Unknown flatfile format.\n");
        break;
    }
    return (index);
}

/**********************************************************/
const char** GetAccArray(Parser::ESource source)
{
    if (source == Parser::ESource::EMBL)
        return (embl_accpref);
    if (source == Parser::ESource::SPROT)
        return (sprot_accpref);
    if (source == Parser::ESource::LANL)
        return (lanl_accpref);
    if (source == Parser::ESource::DDBJ)
        return (ddbj_accpref);
    if (source == Parser::ESource::NCBI)
        return (ncbi_accpref);
    if (source == Parser::ESource::Refseq)
        return (refseq_accpref);
    return nullptr;
}

bool isSupportedAccession(CSeq_id::E_Choice type)
{
    switch (type) {
    case CSeq_id::e_Genbank:
    case CSeq_id::e_Ddbj:
    case CSeq_id::e_Embl:
    case CSeq_id::e_Other:
    case CSeq_id::e_Tpg:
    case CSeq_id::e_Tpd:
    case CSeq_id::e_Tpe:
        return true;
    default:
        break;
    }

    return false;
}


/**********************************************************/
CSeq_id::E_Choice GetNucAccOwner(string_view acc)
{
    auto info = CSeq_id::IdentifyAccession(acc);
    if (CSeq_id::fAcc_prot & info) {
        return CSeq_id::e_not_set;
    }

    if (auto type = CSeq_id::GetAccType(info);
        isSupportedAccession(type)) {
        return type;
    }

    return CSeq_id::e_not_set;
}


/**********************************************************/
CSeq_id::E_Choice GetProtAccOwner(string_view acc)
{
    auto info = CSeq_id::IdentifyAccession(acc);
    if (CSeq_id::fAcc_prot & info) {
        if (auto type = CSeq_id::GetAccType(info);
            isSupportedAccession(type)) {
            return type;
        }
    }

    return CSeq_id::e_not_set;
}

END_NCBI_SCOPE
