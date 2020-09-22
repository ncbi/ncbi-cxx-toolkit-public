/* utilfun.c
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
 * File Name:  utilfun.c
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 *      Utility functions for parser and indexing.
 *
 */
#include <ncbi_pch.hpp>

#include "ftacpp.hpp"

#include <corelib/ncbistr.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <corelib/tempstr.hpp>

#include <objtools/flatfile/index.h>

#include "ftaerr.hpp"
#include "indx_def.h"
#include "utilfun.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "utilfun.cpp"

BEGIN_NCBI_SCOPE;

USING_SCOPE(objects);

CScope& GetScope()
{
    static CScope scope(*CObjectManager::GetInstance());
    return scope;
}


static const char *ParFlat_EST_kw_array[] = {
    "EST",
    "EST PROTO((expressed sequence tag)",
    "expressed sequence tag",
    "EST (expressed sequence tag)",
    "EST (expressed sequence tags)",
    "EST(expressed sequence tag)",
    "transcribed sequence fragment",
    NULL
};

static const char *ParFlat_GSS_kw_array[] = {
    "GSS",
    "GSS (genome survey sequence)",
    "trapped exon",
    NULL
};

static const char *ParFlat_STS_kw_array[] = {
    "STS",
    "STS(sequence tagged site)",
    "STS (sequence tagged site)",
    "STS sequence",
    "sequence tagged site",
    NULL
};

static const char *ParFlat_HTC_kw_array[] = {
    "HTC",
    NULL
};

static const char *ParFlat_FLI_kw_array[] = {
    "FLI_CDNA",
    NULL
};

static const char *ParFlat_WGS_kw_array[] = {
    "WGS",
    NULL
};

static const char *ParFlat_MGA_kw_array[] = {
    "MGA",
    "CAGE (Cap Analysis Gene Expression)",
    "5'-SAGE",
    NULL
};

static const char *ParFlat_MGA_more_kw_array[] = {
    "CAGE (Cap Analysis Gene Expression)",
    "5'-SAGE",
    "5'-end tag",
    "unspecified tag",
    "small RNA",
    NULL
};

/* Any change of contents of next array below requires proper
 * modifications in function fta_tsa_keywords_check().
 */
static const char *ParFlat_TSA_kw_array[] = {
    "TSA",
    "Transcriptome Shotgun Assembly",
    NULL
};

/* Any change of contents of next array below requires proper
 * modifications in function fta_tls_keywords_check().
 */
static const char *ParFlat_TLS_kw_array[] = {
    "TLS",
    "Targeted Locus Study",
    NULL
};

/* Any change of contents of next 2 arrays below requires proper
 * modifications in function fta_tpa_keywords_check().
 */
static const char *ParFlat_TPA_kw_array[] = {
    "TPA",
    "THIRD PARTY ANNOTATION",
    "THIRD PARTY DATA",
    "TPA:INFERENTIAL",
    "TPA:EXPERIMENTAL",
    "TPA:REASSEMBLY",
    "TPA:ASSEMBLY",
    "TPA:SPECIALIST_DB",
    NULL
};

static const char *ParFlat_TPA_kw_array_to_remove[] = {
    "TPA",
    "THIRD PARTY ANNOTATION",
    "THIRD PARTY DATA",
    NULL
};

static const char *ParFlat_ENV_kw_array[] = {
    "ENV",
    NULL
};

/**********************************************************/
static std::string FTAitoa(Int4 m)
{
    Int4 sign = (m < 0) ? -1 : 1;
    std::string res;

    for(m *= sign; m > 9; m /= 10)
        res += m % 10 + '0';

    res += m + '0';

    if(sign < 0)
        res += '-';

    std::reverse(res.begin(), res.end());
    return res;
}

/**********************************************************/
void UnwrapAccessionRange(const objects::CGB_block::TExtra_accessions& extra_accs, objects::CGB_block::TExtra_accessions& hist)
{
    Int4       num1;
    Int4       num2;

    objects::CGB_block::TExtra_accessions ret;

    ITERATE(objects::CGB_block::TExtra_accessions, acc, extra_accs)
    {
        std::string str = *acc;
        if (str.empty())
            continue;

        size_t dash = str.find('-');
        if (dash == std::string::npos)
        {
            ret.push_back(str);
            continue;
        }

        std::string first(str.begin(), str.begin() + dash),
                    last(str.begin() + dash + 1, str.end());
        size_t acclen = first.size();

        const Char* p = first.c_str();
        for (; (*p >= 'A' && *p <= 'Z') || *p == '_';)
            p++;

        size_t preflen = p - first.c_str();

        std::string prefix = first.substr(0, preflen);
        while(*p == '0')
            p++;

        const Char* q = p;
        for (q = p; *p >= '0' && *p <= '9';)
            p++;
        num1 = atoi(q);

        for (p = last.c_str() + preflen; *p == '0';)
            p++;
        for(q = p; *p >= '0' && *p <= '9';)
            p++;
        num2 = atoi(q);

        ret.push_back(first);

        if(num1 == num2)
            continue;

        for (num1++; num1 <= num2; num1++)
        {
            std::string new_acc = prefix;

            std::string num_str = FTAitoa(num1);
            size_t j = acclen - preflen - num_str.size();

            for(size_t i = 0; i < j; i++)
                new_acc += '0';

            new_acc += num_str;
            ret.push_back(new_acc);
        }
    }

    ret.swap(hist);
}

/**********************************************************/
bool ParseAccessionRange(TokenStatBlkPtr tsbp, Int4 skip)
{
    TokenBlkPtr tbp;
    TokenBlkPtr tbpnext;
    char*     dash;
    char*     first;
    char*     last;
    char*     p;
    char*     q;
    bool        bad;
    Int4        num1;
    Int4        num2;

    if(tsbp->list == NULL)
        return true;

    tbp = NULL;
    if(skip == 0)
        tbp = tsbp->list;
    else if(skip == 1)
    {
        if(tsbp->list != NULL)
            tbp = tsbp->list->next;
    }
    else
    {
        if(tsbp->list != NULL && tsbp->list->next != NULL)
            tbp = tsbp->list->next->next;
    }
    if(tbp == NULL)
        return true;

    for(bad = false; tbp != NULL; tbp = tbpnext)
    {
        tbpnext = tbp->next;
        if(tbp->str == NULL)
            continue;
        dash = StringChr(tbp->str, '-');
        if(dash == NULL)
            continue;
        *dash = '\0';
        first = tbp->str;
        last = dash + 1;
        if(StringLen(first) != StringLen(last) || *first < 'A' ||
           *first > 'Z' || *last < 'A' || *last > 'Z')
        {
            *dash = '-';
            bad = true;
            break;
        }

        for(p = first; (*p >= 'A' && *p <= 'Z') || *p == '_';)
            p++;
        if(*p < '0' || *p > '9')
        {
            *dash = '-';
            bad = true;
            break;
        }
        for(q = last; (*q >= 'A' && *q <= 'Z') || *q == '_';)
            q++;
        if(*q < '0' || *q > '9')
        {
            *dash = '-';
            bad = true;
            break;
        }
        size_t preflen = p - first;
        if(preflen != (size_t) (q - last) || StringNCmp(first, last, preflen) != 0)
        {
            *dash = '-';
            ErrPostEx(SEV_REJECT, ERR_ACCESSION_2ndAccPrefixMismatch,
                      "Inconsistent prefix found in secondary accession range \"%s\".",
                      tbp->str);
            break;
        }

        while(*p == '0')
            p++;
        for(q = p; *p >= '0' && *p <= '9';)
            p++;
        if(*p != '\0')
        {
            *dash = '-';
            bad = true;
            break;
        }
        num1 = atoi(q);

        for(p = last + preflen; *p == '0';)
            p++;
        for(q = p; *p >= '0' && *p <= '9';)
            p++;
        if(*p != '\0')
        {
            *dash = '-';
            bad = true;
            break;
        }
        num2 = atoi(q);

        if(num1 > num2)
        {
            *dash = '-';
            ErrPostEx(SEV_REJECT, ERR_ACCESSION_Invalid2ndAccRange,
                      "Invalid start/end values in secondary accession range \"%s\".",
                      tbp->str);
            break;
        }

        tbp->next = (TokenBlkPtr) MemNew(sizeof(TokenBlk));
        tbp = tbp->next;
        tbp->str = StringSave("-");
        tbp->next = (TokenBlkPtr) MemNew(sizeof(TokenBlk));
        tbp = tbp->next;
        tbp->str = StringSave(last);
        tsbp->num += 2;

        tbp->next = tbpnext;
    }
    if(tbp == NULL)
        return true;
    if(bad)
    {
        ErrPostEx(SEV_REJECT, ERR_ACCESSION_Invalid2ndAccRange,
                  "Incorrect secondary accession range provided: \"%s\".",
                  tbp->str);
    }
    return false;
}

/**********************************************************/
static TokenBlkPtr TokenNodeNew(TokenBlkPtr tbp)
{
    TokenBlkPtr newnode = (TokenBlkPtr) MemNew(sizeof(TokenBlk));

    if(tbp != NULL)
    {
        while(tbp->next != NULL)
            tbp = tbp->next;
        tbp->next = newnode;
    }

    return(newnode);
}

/**********************************************************/
static void InsertTokenVal(TokenBlkPtr* tbp, char* str)
{
    TokenBlkPtr ltbp;

    ltbp = *tbp;
    ltbp = TokenNodeNew(ltbp);
    ltbp->str = StringSave(str);

    if(*tbp == NULL)
        *tbp = ltbp;
}

/**********************************************************
 *
 *   TokenStatBlkPtr TokenString(str, delimiter):
 *
 *      Parsing string "str" by delimiter or tab key, blank.
 *      Parsing stop at newline ('\n') or end of string ('\0').
 *      Return a statistics of link list token.
 *
 **********************************************************/
TokenStatBlkPtr TokenString(char* str, Char delimiter)
{
    char*         bptr;
    char*         ptr;
    char*         curtoken;
    Int2            num;
    TokenStatBlkPtr token;
    Char            ch;

    token = (TokenStatBlkPtr) MemNew(sizeof(TokenStatBlk));

    /* skip first several delimiters if any existed
     */
    for(ptr = str; *ptr == delimiter;)
        ptr++;

    for(num = 0; *ptr != '\0' && *ptr != '\n';)
    {
        for(bptr = ptr; *ptr != delimiter && *ptr != '\n' &&
            *ptr != '\t' && *ptr != ' ' && *ptr != '\0';)
            ptr++;

        ch = *ptr;
        *ptr = '\0';
        curtoken = StringSave(bptr);
        *ptr = ch;

        InsertTokenVal(&token->list, curtoken);
        num++;
        MemFree(curtoken);

        while(*ptr == delimiter || *ptr == '\t' || *ptr == ' ')
            ptr++;
    }

    token->num = num;

    return(token);
}

/**********************************************************
 *
 *   TokenStatBlkPtr TokenStringByDelimiter(str, delimiter):
 *
 *      Parsing string "str" by delimiter.
 *      Parsing stop at end of string ('\0').
 *      Return a statistics of link list token.
 *
 **********************************************************/
TokenStatBlkPtr TokenStringByDelimiter(char* str, Char delimiter)
{
    char*         bptr;
    char*         ptr;
    char*         curtoken;
    char*         s;
    Int2            num;
    TokenStatBlkPtr token;
    Char            ch;

    token = (TokenStatBlkPtr) MemNew(sizeof(TokenStatBlk));

    /* skip first several delimiters if any existed
     */
    for(ptr = str; *ptr == delimiter;)
        ptr++;

    /* remove '.' from the end of the string
     */
    s = ptr + StringLen(ptr) - 1;
    if(*s == '.')
        *s = '\0';

    for(num = 0; *ptr != '\0';)
    {
        for(bptr = ptr; *ptr != delimiter && *ptr != '\0';)
            ptr++;

        ch = *ptr;
        *ptr = '\0';
        curtoken = StringSave(bptr);
        *ptr = ch;

        InsertTokenVal(&token->list, curtoken);
        num++;
        MemFree(curtoken);

        while(*ptr == delimiter || *ptr == ' ')
            ptr++;
    }

    token->num = num;

    return(token);
}

/**********************************************************/
void FreeTokenblk(TokenBlkPtr tbp)
{
    TokenBlkPtr temp;

    while(tbp != NULL)
    {
        temp = tbp;
        tbp = tbp->next;
        MemFree(temp->str);
        MemFree(temp);
    }
}

/**********************************************************/
void FreeTokenstatblk(TokenStatBlkPtr tsbp)
{
    FreeTokenblk(tsbp->list);
    MemFree(tsbp);
}

/**********************************************************
 *
 *   Int2 fta_StringMatch(array, text):
 *
 *      Return array position of the matched length
 *   of string in array.
 *      Return -1 if no match.
 *
 **********************************************************/
Int2 fta_StringMatch(const Char **array, const Char* text)
{
    Int2 i;

    if(text == NULL)
        return(-1);

    for (i = 0; *array != NULL; i++, array++)
    {
        if (NStr::EqualCase(text, 0, StringLen(*array), *array))
            break;
    }

    if(*array == NULL)
        return(-1);

    return(i);
}

/**********************************************************
 *
 *   Int2 StringMatchIcase(array, text):
 *
 *      Return array position of the matched lenght of
 *   string (ignored case) in array.
 *      Return -1 if no match.
 *
 **********************************************************/
Int2 StringMatchIcase(const Char **array, const Char* text)
{
    Int2 i;

    if(text == NULL)
        return(-1);

    for (i = 0; *array != NULL; i++, array++)
    {
        // If string from an array is empty its length == 0 and would be equval to any other string
        // The next 'if' statement will avoid that behavior
        if (text[0] != 0 && *array[0] == 0)
            continue;

        if (NStr::EqualNocase(text, 0, StringLen(*array), *array))
            break;
    }

    if(*array == NULL)
        return(-1);
    return(i);
}

/**********************************************************
 *
 *   Int2 MatchArrayString(array, text):
 *
 *      Return array position of the string in the
 *   array.
 *      Return -1 if no match.
 *
 **********************************************************/
Int2 MatchArrayString(const char **array, const char *text)
{
    Int2 i;

    if(text == NULL)
        return(-1);

    for (i = 0; *array != NULL; i++, array++)
    {
        if (NStr::Equal(*array, text))
            break;
    }

    if(*array == NULL)
        return(-1);
    return(i);
}

/**********************************************************/
Int2 MatchArrayIString(const Char **array, const Char *text)
{
    Int2 i;

    if(text == NULL)
        return(-1);

    for (i = 0; *array != NULL; i++, array++)
    {
        // If string from an array is empty its length == 0 and would be equval to any other string
        // The next 'if' statement will avoid that behavior
        if (text[0] != 0 && *array[0] == 0)
            continue;

        if (NStr::EqualNocase(*array, text))
            break;
    }

    if(*array == NULL)
        return(-1);
    return(i);
}

/**********************************************************
 *
 *   Int2 MatchArraySubString(array, text):
 *
 *      Return array position of the string in the array
 *   if any array is in the substring of "text".
 *      Return -1 if no match.
 *
 **********************************************************/
Int2 MatchArraySubString(const Char **array, const Char* text)
{
    Int2 i;

    if(text == NULL)
        return(-1);

    for (i = 0; *array != NULL; i++, array++)
    {
        if (NStr::Find(text, *array) != NPOS)
            break;
    }

    if(*array == NULL)
        return(-1);
    return(i);
}

/**********************************************************/
Char* StringIStr(const Char* where, const Char *what)
{
    const Char* p;
    const Char* q;

    if(where == NULL || *where == '\0' || what == NULL || *what == '\0')
        return(NULL);

    q = NULL;
    for(; *where != '\0'; where++)
    {
        for(q = what, p = where; *q != '\0' && *p != '\0'; q++, p++)
        {
            if(*q == *p)
                continue;

            if(*q >= 'A' && *q <= 'Z')
            {
                if(*q + 32 == *p)
                    continue;
            }
            else if(*q >= 'a' && *q <= 'z')
            {
                if(*q - 32 == *p)
                    continue;
            }
            break;
        }
        if(*p == '\0' || *q == '\0')
            break;
    }
    if(q != NULL && *q == '\0')
        return const_cast<char*>(where);
    return(NULL);
}

/**********************************************************/
Int2 MatchArrayISubString(const Char **array, const Char* text)
{
    Int2 i;

    if(text == NULL)
        return(-1);

    for (i = 0; *array != NULL; i++, array++)
    {
        if (NStr::FindNoCase(text, *array) != NPOS)
            break;
    }

    if(*array == NULL)
        return(-1);
    return(i);
}

/**********************************************************
 *
 *   char* GetBlkDataReplaceNewLine(bptr, eptr,
 *                                    start_col_data):
 *
 *      Return a string which replace newline to blank
 *   and skip "XX" line data.
 *
 **********************************************************/
char* GetBlkDataReplaceNewLine(char* bptr, char* eptr,
                                 Int2 start_col_data)
{
    char* ptr;

    if(bptr + start_col_data >= eptr)
        return(NULL);

    size_t size = eptr - bptr;
    char* retstr = (char*)MemNew(size + 1);
    char* str = retstr;

    while(bptr < eptr)
    {
        if (NStr::Equal(bptr, 0, 2, "XX"))      /* skip XX line data */
        {
            ptr = SrchTheChar(bptr, eptr, '\n');
            bptr = ptr + 1;
            continue;
        }

        bptr += start_col_data;
        ptr = SrchTheChar(bptr, eptr, '\n');

        if(ptr != NULL)
        {
            size = ptr - bptr;
            MemCpy(str, bptr, size);
            str += size;
            if(*(ptr - 1) != '-' || *(ptr - 2) == ' ')
            {
                StringCpy(str, " ");
                str++;
            }
            bptr = ptr;
        }
        bptr++;
    }

    std::string tstr = NStr::TruncateSpaces(std::string(retstr), NStr::eTrunc_End);
    MemFree(retstr);
    retstr = StringSave(tstr.c_str());

    return(retstr);
}


/**********************************************************/
static size_t SeekLastAlphaChar(const Char* str, size_t len)
{
    size_t ret = 0;
    if (str != NULL && len != 0)
    {
        for (ret = len - 1; ret >= 0; --ret)
        {
            if (str[ret] != ' ' && str[ret] != '\n' && str[ret] != '\\' && str[ret] != ',' &&
                str[ret] != ';' && str[ret] != '~' && str[ret] != '.' && str[ret] != ':')
            {
                ++ret;
                break;
            }
        }

        if (ret < 0)
            ret = 0;
    }

    return ret;
}

/**********************************************************/
void CleanTailNoneAlphaCharInString(std::string& str)
{
    size_t ret = SeekLastAlphaChar(str.c_str(), str.size());
    str = str.substr(0, ret);
}

/**********************************************************
 *
 *   void CleanTailNoneAlphaChar(str):
 *
 *      Delete any tailing ' ', '\n', '\\', ',', ';', '~',
 *   '.', ':' characters.
 *
 **********************************************************/
void CleanTailNoneAlphaChar(char* str)
{
    if(str == NULL || *str == '\0')
        return;

    size_t last = SeekLastAlphaChar(str, strlen(str));
    str[last] = '\0';
}

/**********************************************************/
char* PointToNextToken(char* ptr)
{
    if(ptr != NULL)
    {
        while(*ptr != ' ')
            ptr++;
        while(*ptr == ' ')
            ptr++;
    }
    return(ptr);
}

/**********************************************************
 *
 *   char* GetTheCurrentToken(ptr):
 *
 *      Return the current token (also CleanTailNoneAlphaChar)
 *   which ptr points to and ptr will points to next token
 *   after the routine return.
 *
 **********************************************************/
char* GetTheCurrentToken(char** ptr)
{
    char* retptr;
    char* bptr;
    char* str;
    Char    ch;

    bptr = retptr = *ptr;
    if(retptr == NULL || *retptr == '\0')
        return(NULL);

    while(*retptr != '\0' && *retptr != ' ')
        retptr++;

    ch = *retptr;
    *retptr = '\0';
    str = StringSave(bptr);
    *retptr = ch;

    while(*retptr != '\0' && *retptr == ' ')    /* skip blanks */
        retptr++;
    *ptr = retptr;

    CleanTailNoneAlphaChar(str);
    return(str);
}

/**********************************************************
 *
 *   char* SrchTheChar(bptr, eptr, letter):
 *
 *      Search The character letter.
 *      Return NULL if not found; otherwise, return
 *   a pointer points first occurrence The character.
 *
 **********************************************************/
char* SrchTheChar(char* bptr, char* eptr, Char letter)
{
    while(bptr < eptr && *bptr != letter)
        bptr++;

    if(bptr < eptr)
        return(bptr);

    return(NULL);
}

/**********************************************************
 *
 *   char* SrchTheStr(bptr, eptr, leadstr):
 *
 *      Search The leading string.
 *      Return NULL if not found; otherwise, return
 *   a pointer points first occurrence The leading string.
 *
 **********************************************************/
char* SrchTheStr(char* bptr, char* eptr, const char *leadstr)
{
    char* p;
    Char    c;

    c = *eptr;
    *eptr = '\0';
    p = StringStr(bptr, leadstr);
    *eptr = c;
    return(p);
}

/**********************************************************/
void CpSeqId(InfoBioseqPtr ibp, const objects::CSeq_id& id)
{
    const objects::CTextseq_id* text_id = id.GetTextseq_Id();
    if (text_id != nullptr)
    {
        if (text_id->IsSetName())
            ibp->locus = StringSave(text_id->GetName().c_str());

        CRef<objects::CSeq_id> new_id(new objects::CSeq_id);
        if (text_id->IsSetAccession())
        {
            ibp->acnum = StringSave(text_id->GetAccession().c_str());

            CRef<objects::CTextseq_id> new_text_id(new objects::CTextseq_id);
            new_text_id->SetAccession(text_id->GetAccession());
            if (text_id->IsSetVersion())
                new_text_id->SetVersion(text_id->GetVersion());

            SetTextId(id.Which(), *new_id, *new_text_id);
        }
        else
        {
            new_id->Assign(id);
        }

        ibp->ids.push_back(new_id);
    }
}

/**********************************************************/
void InfoBioseqFree(InfoBioseqPtr ibp)
{
    if (!ibp->ids.empty())
        ibp->ids.clear();

    if(ibp->locus != NULL)
    {
        MemFree(ibp->locus);
        ibp->locus = NULL;
    }

    if(ibp->acnum != NULL)
    {
        MemFree(ibp->acnum);
        ibp->acnum = NULL;
    }
}

/**********************************************************
    *
    *   CRef<objects::CDate_std> get_full_date(s, is_ref, source):
    *
    *      Get year, month, day and return CRef<objects::CDate_std>.
    *
    **********************************************************/
CRef<objects::CDate_std> get_full_date(const Char* s, bool is_ref, Int2 source)
{
    static const char *months[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };

    int               day = 0;
    int               month = 0;
    int               year;
    int               cal;
    Char           msg[11];
    const Char*        p;

    CRef<objects::CDate_std> date;

    if (s == NULL || *s == '\0')
        return date;

    if (IS_DIGIT(*s) != 0)
    {
        day = atoi(s);
        s += 3;
    }

    int num_of_months = sizeof(months) / sizeof(months[0]);
    for (cal = 0; cal < num_of_months; cal++)
    {
        if (StringNICmp(s, months[cal], 3) != 0)
            continue;
        month = cal + 1;
        break;
    }

    if (cal == num_of_months)
    {
        StringNCpy(msg, s, 10);
        msg[10] = '\0';
        if (is_ref)
            ErrPostEx(SEV_WARNING, ERR_REFERENCE_IllegalDate,
            "Unrecognized month: %s", msg);
        else
            ErrPostEx(SEV_WARNING, ERR_DATE_IllegalDate,
            "Unrecognized month: %s", msg);
        return date;
    }
    p = s + 4;

    date = new objects::CDate_std;
    year = atoi(p);
    if ((StringNCmp(p, "19", 2) == 0 || StringNCmp(p, "20", 2) == 0 ||
         StringNCmp(p, "20", 2) == 0) &&
        p[2] >= '0' && p[2] <= '9' && p[3] >= '0' && p[3] <= '9')
    {
        CTime cur_time(CTime::eCurrent);
        objects::CDate_std cur(cur_time);
        objects::CDate_std::TYear cur_year = cur.GetYear();

        if (year < 1900 || year > cur_year)
        {
            if (is_ref)
                ErrPostEx(SEV_ERROR, ERR_REFERENCE_IllegalDate,
                "Illegal year: %d, current year: %d", year, cur_year);
            else
            {
                if (source != ParFlat_SPROT || year - cur_year > 1)
                    ErrPostEx(SEV_WARNING, ERR_DATE_IllegalDate,
                    "Illegal year: %d, current year: %d", year, cur_year);
            }
        }

        date->SetYear(year);
    }
    else
    {
        if (year < 70)
            year += 2000;
        else
            year += 1900;
        date->SetYear(year);
    }

    date->SetMonth(month);
    date->SetDay(day);

    return date;
}

/**********************************************************
 *
 *   Int2 SrchKeyword(ptr, kwl):
 *
 *      Compare first kwl.len byte in ptr to kwl.str.
 *      Return the position of keyword block array;
 *   return unknow keyword, UNKW, if not found.
 *
 *                                              3-25-93
 *
 **********************************************************/
Int2 SrchKeyword(char* ptr, KwordBlk kwl[])
{
    Int2 i;

    for(i = 0; kwl[i].str != NULL; i++)
        if(StringNCmp(ptr, kwl[i].str, kwl[i].len) == 0)
            break;

    if(kwl[i].str == NULL)
        return(ParFlat_UNKW);
    return(i);
}

/**********************************************************/
bool CheckLineType(char* ptr, Int4 line, KwordBlk kwl[], bool after_origin)
{
    char* p;
    Char    msg[51];
    Int2    i;

    if(after_origin)
    {
        for(p = ptr; *p >= '0' && *p <= '9';)
            p++;
        if(*p == ' ')
            return true;
    }

    for(i = 0; kwl[i].str != NULL; i++)
        if(StringNCmp(ptr, kwl[i].str, kwl[i].len) == 0)
            break;
    if(kwl[i].str != NULL)
        return true;

    StringNCpy(msg, StringSave(ptr), 50);
    msg[50] = '\0';
    p = StringChr(msg, '\n');
    if(p != NULL)
        *p = '\0';
    ErrPostEx(SEV_ERROR, ERR_ENTRY_InvalidLineType,
              "Unknown linetype \"%s\". Line number %d.", msg, line);
    if(p != NULL)
        *p = '\n';

    return false;
}

/**********************************************************
 *
 *   char* SrchNodeType(entry, type, len):
 *
 *      Return a memory location of the node which has
 *   the "type".
 *
 **********************************************************/
char* SrchNodeType(DataBlkPtr entry, Int4 type, size_t* len)
{
    DataBlkPtr temp;

    temp = TrackNodeType(entry, (Int2) type);
    if(temp != NULL)
    {
        *len = temp->len;
        return(temp->offset);
    }

    *len = 0;
    return(NULL);
}

/**********************************************************
 *
 *   DataBlkPtr TrackNodeType(entry, type):
 *
 *      Return a pointer points to the Node which has
 *   the "type".
 *
 **********************************************************/
DataBlkPtr TrackNodeType(DataBlkPtr entry, Int2 type)
{
    DataBlkPtr  temp;
    EntryBlkPtr ebp;

    ebp = (EntryBlkPtr) entry->data;
    temp = (DataBlkPtr) ebp->chain;
    while(temp != NULL && temp->type != type)
        temp = temp->next;

    return(temp);
}

/**********************************************************/
bool fta_tpa_keywords_check(const TKeywordList& kwds)
{
    const char* b[4];

    bool kwd_tpa = false;
    bool kwd_party = false;
    bool kwd_inf = false;
    bool kwd_exp = false;
    bool kwd_asm = false;
    bool kwd_spedb = false;
    bool ret = true;

    Int4    j;
    Int2    i;

    if(kwds.empty())
        return true;

    size_t len = 0;
    j = 0;
    ITERATE(TKeywordList, key, kwds)
    {
        if(key->empty())
            continue;

        const char* p = key->c_str();
        i = MatchArrayIString(ParFlat_TPA_kw_array, p);
        if(i == 0)
            kwd_tpa = true;
        else if(i == 1 || i == 2)
            kwd_party = true;
        else if(i == 3)
            kwd_inf = true;
        else if(i == 4)
            kwd_exp = true;
        else if(i == 5 || i == 6)
            kwd_asm = true;
        else if(i == 7)
            kwd_spedb = true;
        else if (NStr::EqualNocase(p, 0, 3, "TPA"))
        {
            if(p[3] == ':')
            {
                ErrPostEx(SEV_REJECT, ERR_KEYWORD_InvalidTPATier,
                          "Keyword \"%s\" is not a valid TPA-tier keyword.",
                          p);
                ret = false;
            }
            else if(p[3] != '\0' && p[4] != '\0')
            {
                ErrPostEx(SEV_WARNING, ERR_KEYWORD_UnexpectedTPA,
                          "Keyword \"%s\" looks like it might be TPA-related, but it is not a recognized TPA keyword.",
                          p);
            }
        }
        if(i > 2 && i < 8 && j < 4)
        {
            b[j] = p;
            ++j;
            len += key->size() + 1;
        }
    }

    if(kwd_tpa && !kwd_party)
    {
        ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingTPAKeywords,
                  "This TPA-record should have keyword \"Third Party Annotation\" or \"Third Party Data\" in addition to \"TPA\".");
        ret = false;
    }
    else if(!kwd_tpa && kwd_party)
    {
        ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingTPAKeywords,
                  "This TPA-record should have keyword \"TPA\" in addition to \"Third Party Annotation\" or \"Third Party Data\".");
        ret = false;
    }
    if(!kwd_tpa && (kwd_inf || kwd_exp))
    {
        ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingTPAKeywords,
                  "This TPA-record should have keyword \"TPA\" in addition to its TPA-tier keyword.");
        ret = false;
    }
    else if(kwd_tpa && kwd_inf == false && kwd_exp == false &&
            kwd_asm == false && kwd_spedb == false)
    {
        ErrPostEx(SEV_ERROR, ERR_KEYWORD_MissingTPATier,
                  "This TPA record lacks a keyword to indicate which tier it belongs to: experimental, inferential, reassembly or specialist_db.");
    }
    if(j > 1)
    {
        std::string buf;
        for(i = 0; i < j; i++)
        {
            if(i > 0)
                buf += ';';
            buf += b[i];
        }
        ErrPostEx(SEV_REJECT, ERR_KEYWORD_ConflictingTPATiers,
                  "Keywords for multiple TPA tiers exist on this record: \"%s\". A TPA record can only be in one tier.",
                  buf.c_str());
        ret = false;
    }

    return(ret);
}

/**********************************************************/
bool fta_tsa_keywords_check(const TKeywordList& kwds, Int2 source)
{
    bool kwd_tsa = false;
    bool kwd_assembly = false;
    bool ret = true;
    Int2 i;

    if(kwds.empty())
        return true;

    ITERATE(TKeywordList, key, kwds)
    {
        if(key->empty())
            continue;
        i = MatchArrayIString(ParFlat_TSA_kw_array, key->c_str());
        if(i == 0)
            kwd_tsa = true;
        else if(i == 1)
            kwd_assembly = true;
        else if(source == ParFlat_EMBL &&
                NStr::EqualNocase(*key, "Transcript Shotgun Assembly"))
            kwd_assembly = true;
    }

    if(kwd_tsa && !kwd_assembly)
    {
        ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingTSAKeywords,
                  "This TSA-record should have keyword \"Transcriptome Shotgun Assembly\" in addition to \"TSA\".");
        ret = false;
    }
    else if(!kwd_tsa && kwd_assembly)
    {
        ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingTSAKeywords,
                  "This TSA-record should have keyword \"TSA\" in addition to \"Transcriptome Shotgun Assembly\".");
        ret = false;
    }
    return(ret);
}

/**********************************************************/
bool fta_tls_keywords_check(const TKeywordList& kwds, Int2 source)
{
    bool kwd_tls = false;
    bool kwd_study = false;
    bool ret = true;
    Int2 i;

    if(kwds.empty())
        return true;

    ITERATE(TKeywordList, key, kwds)
    {
        if(key->empty())
            continue;
        i = MatchArrayIString(ParFlat_TLS_kw_array, key->c_str());
        if(i == 0)
            kwd_tls = true;
        else if(i == 1)
            kwd_study = true;
        else if(source == ParFlat_EMBL &&
                NStr::EqualNocase(*key, "Targeted Locus Study"))
            kwd_study = true;
    }

    if(kwd_tls && !kwd_study)
    {
        ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingTLSKeywords,
                  "This TLS-record should have keyword \"Targeted Locus Study\" in addition to \"TLS\".");
        ret = false;
    }
    else if(!kwd_tls && kwd_study)
    {
        ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingTLSKeywords,
                  "This TLS-record should have keyword \"TLS\" in addition to \"Targeted Locus Study\".");
        ret = false;
    }
    return(ret);
}

/**********************************************************/
bool fta_is_tpa_keyword(const char* str)
{
    if(str == NULL || *str == '\0' || MatchArrayIString(ParFlat_TPA_kw_array, str) < 0)
        return false;

    return true;
}

/**********************************************************/
bool fta_is_tsa_keyword(char* str)
{
    if(str == NULL || *str == '\0' || MatchArrayIString(ParFlat_TSA_kw_array, str) < 0)
        return false;
    return true;
}

/**********************************************************/
bool fta_is_tls_keyword(char* str)
{
    if(str == NULL || *str == '\0' || MatchArrayIString(ParFlat_TLS_kw_array, str) < 0)
        return false;
    return true;
}

/**********************************************************/
void fta_keywords_check(const char* str, bool* estk, bool* stsk, bool* gssk,
                        bool* htck, bool* flik, bool* wgsk, bool* tpak,
                        bool* envk, bool* mgak, bool* tsak, bool* tlsk)
{
    if(estk != NULL && MatchArrayString(ParFlat_EST_kw_array, str) != -1)
        *estk = true;

    if(stsk != NULL && MatchArrayString(ParFlat_STS_kw_array, str) != -1)
        *stsk = true;

    if(gssk != NULL && MatchArrayString(ParFlat_GSS_kw_array, str) != -1)
        *gssk = true;

    if(htck != NULL && MatchArrayString(ParFlat_HTC_kw_array, str) != -1)
        *htck = true;

    if(flik != NULL && MatchArrayString(ParFlat_FLI_kw_array, str) != -1)
        *flik = true;

    if(wgsk != NULL && MatchArrayString(ParFlat_WGS_kw_array, str) != -1)
        *wgsk = true;

    if(tpak != NULL && MatchArrayString(ParFlat_TPA_kw_array, str) != -1)
        *tpak = true;

    if(envk != NULL && MatchArrayString(ParFlat_ENV_kw_array, str) != -1)
        *envk = true;

    if(mgak != NULL && MatchArrayString(ParFlat_MGA_kw_array, str) != -1)
        *mgak = true;

    if(tsak != NULL && MatchArrayString(ParFlat_TSA_kw_array, str) != -1)
        *tsak = true;

    if(tlsk != NULL && MatchArrayString(ParFlat_TLS_kw_array, str) != -1)
        *tlsk = true;
}

/**********************************************************/
void fta_remove_keywords(Uint1 tech, TKeywordList& kwds)
{
    const char **b;

    if(kwds.empty())
        return;

    if (tech == objects::CMolInfo::eTech_est)
        b = ParFlat_EST_kw_array;
    else if (tech == objects::CMolInfo::eTech_sts)
        b = ParFlat_STS_kw_array;
    else if (tech == objects::CMolInfo::eTech_survey)
        b = ParFlat_GSS_kw_array;
    else if (tech == objects::CMolInfo::eTech_htc)
        b = ParFlat_HTC_kw_array;
    else if (tech == objects::CMolInfo::eTech_fli_cdna)
        b = ParFlat_FLI_kw_array;
    else if (tech == objects::CMolInfo::eTech_wgs)
        b = ParFlat_WGS_kw_array;
    else
        return;

    for (TKeywordList::iterator key = kwds.begin(); key != kwds.end();)
    {
        if (key->empty() || MatchArrayString(b, key->c_str()) != -1)
        {
            key = kwds.erase(key);
        }
        else
            ++key;
    }
}

/**********************************************************/
void fta_remove_tpa_keywords(TKeywordList& kwds)
{
    if (kwds.empty())
        return;

    for (TKeywordList::iterator key = kwds.begin(); key != kwds.end();)
    {
        if (key->empty() || MatchArrayIString(ParFlat_TPA_kw_array_to_remove, key->c_str()) != -1)
        {
            key = kwds.erase(key);
        }
        else
            ++key;
    }
}

/**********************************************************/
void fta_remove_tsa_keywords(TKeywordList& kwds, Int2 source)
{
    if (kwds.empty())
        return;

    for (TKeywordList::iterator key = kwds.begin(); key != kwds.end();)
    {
        if (key->empty() || MatchArrayIString(ParFlat_TSA_kw_array, key->c_str()) != -1 ||
            (source == ParFlat_EMBL && NStr::EqualNocase(*key, "Transcript Shotgun Assembly")))
        {
            key = kwds.erase(key);
        }
        else
            ++key;
    }
}

/**********************************************************/
void fta_remove_tls_keywords(TKeywordList& kwds, Int2 source)
{
    if (kwds.empty())
        return;

    for (TKeywordList::iterator key = kwds.begin(); key != kwds.end();)
    {
        if (key->empty() || MatchArrayIString(ParFlat_TLS_kw_array, key->c_str()) != -1 ||
            (source == ParFlat_EMBL && NStr::EqualNocase(*key, "Targeted Locus Study")))
        {
            key = kwds.erase(key);
        }
        else
            ++key;
    }
}

/**********************************************************/
void fta_remove_env_keywords(TKeywordList& kwds)
{
    if (kwds.empty())
        return;

    for (TKeywordList::iterator key = kwds.begin(); key != kwds.end();)
    {
        if (key->empty() || MatchArrayIString(ParFlat_ENV_kw_array, key->c_str()) != -1)
        {
            key = kwds.erase(key);
        }
        else
            ++key;
    }
}

/**********************************************************/
void check_est_sts_gss_tpa_kwds(ValNodePtr kwds, size_t len, IndexblkPtr entry,
                                bool tpa_check, bool &specialist_db,
                                bool &inferential, bool &experimental,
                                bool &assembly)
{
    char* line;
    char* p;
    char* q;

    if(kwds == NULL || kwds->data.ptrvalue == NULL || len < 1)
        return;

    line = (char*) MemNew(len + 1);
    line[0] = '\0';
    for(; kwds != NULL; kwds = kwds->next)
    {
        StringCat(line, (const char *) kwds->data.ptrvalue);
    }
    for(p = line; *p != '\0'; p++)
        if(*p == '\n' || *p == '\t')
            *p = ' ';
    for(p = line; *p == ' ' || *p == '.' || *p == ';';)
        p++;
    if(*p == '\0')
    {
        MemFree(line);
        return;
    }
    for(q = p; *q != '\0';)
        q++;
    for(q--; *q == ' ' || *q == '.' || *q == ';'; q--)
        *q = '\0';
    for(q = p, p = line; *q != '\0';)
    {
        if(*q != ' ' && *q != ';')
        {
            *p++ = *q++;
            continue;
        }
        if(*q == ' ')
        {
            for(q++; *q == ' ';)
                q++;
            if(*q != ';')
                *p++ = ' ';
        }
        if(*q == ';')
        {
            *p++ = *q++;
            while(*q == ' ' || *q == ';')
                q++;
        }
    }
    *p++ = ';';
    *p = '\0';
    for(p = line;; p = q + 1)
    {
        q = StringChr(p, ';');
        if(q == NULL)
            break;
        *q = '\0';
        fta_keywords_check(p, &entry->EST, &entry->STS, &entry->GSS,
                           &entry->HTC, NULL, NULL,
                           (tpa_check ? &entry->is_tpa : NULL),
                           NULL, NULL, NULL, NULL);
        if(NStr::EqualNocase(p, "TPA:specialist_db") ||
           NStr::EqualNocase(p, "TPA:assembly"))
        {
            specialist_db = true;
            if(NStr::EqualNocase(p, "TPA:assembly"))
                assembly = true;
        }
        else if(NStr::EqualNocase(p, "TPA:inferential"))
            inferential = true;
        else if(NStr::EqualNocase(p, "TPA:experimental"))
            experimental = true;
    }
    MemFree(line);
}

/**********************************************************/
void fta_operon_free(FTAOperonPtr fop)
{
    FTAOperonPtr fopnext;

    for(; fop != NULL; fop = fopnext)
    {
        fopnext = fop->next;
        if(fop->strloc != NULL)
            MemFree(fop->strloc);
        delete fop;
    }
}

/**********************************************************/
ValNodePtr ConstructValNode(ValNodePtr head, Uint1 choice, void* data)
{
    ValNodePtr res;

    res = ValNodeNew(head);
    res->choice = choice;
    res->data.ptrvalue = data;
    res->next = NULL;
    return(res);
}

/**********************************************************/
ValNodePtr ConstructValNodeInt(ValNodePtr head, Uint1 choice, Int4 data)
{
    ValNodePtr res;

    res = ValNodeNew(head);
    res->choice = choice;
    res->data.intvalue = data;
    res->next = NULL;
    return(res);
}

/**********************************************************/
bool fta_check_mga_keywords(objects::CMolInfo& mol_info, const TKeywordList& kwds)
{
    bool is_cage;
    bool is_sage;

    TKeywordList::const_iterator key_it = kwds.end();

    bool got = false;
    if (!kwds.empty() && NStr::EqualNocase(kwds.front(), "MGA"))
    {
        ITERATE(TKeywordList, key, kwds)
        {
            if(MatchArrayIString(ParFlat_MGA_more_kw_array,
                                 key->c_str()) < 0)
                continue;
            got = true;
            key_it = key;
            break;
        }
    }

    if(!got)
    {
        ErrPostEx(SEV_REJECT, ERR_KEYWORD_MissingMGAKeywords,
                  "This is apparently a CAGE record, but it lacks the required keywords. Entry dropped.");
        return false;
    }

    if (!mol_info.IsSetTechexp() || !kwds.empty() ||
        mol_info.GetTechexp() != "cage")
        return true;

    for (is_sage = false, is_cage = false; key_it != kwds.end(); ++key_it)
    {
        const char* p = key_it->c_str();

        if (NStr::EqualNocase(p, "5'-SAGE"))
            is_sage = true;
        else if (NStr::EqualNocase(p, "CAGE (Cap Analysis Gene Expression)"))
            is_cage = true;
    }

    if(is_sage)
    {
        if(is_cage)
        {
            ErrPostEx(SEV_REJECT, ERR_KEYWORD_ConflictingMGAKeywords,
                      "This MGA record contains more than one of the special keywords indicating different techniques.");
            return false;
        }
        mol_info.SetTechexp("5'-sage");
    }

    return true;
}

/**********************************************************/
void fta_StringCpy(char* dst, char* src)
{
    char* p;
    char* q;

    for(q = dst, p = src; *p != '\0';)
        *q++ = *p++;
    *q = '\0';
}

/**********************************************************/
bool SetTextId(Uint1 seqtype, objects::CSeq_id& seqId, objects::CTextseq_id& textId)
{
    bool wasSet = true;

    switch (seqtype)
    {
    case objects::CSeq_id::e_Genbank:
        seqId.SetGenbank(textId);
        break;
    case objects::CSeq_id::e_Embl:
        seqId.SetEmbl(textId);
        break;
    case objects::CSeq_id::e_Pir:
        seqId.SetPir(textId);
        break;
    case objects::CSeq_id::e_Swissprot:
        seqId.SetSwissprot(textId);
        break;
    case objects::CSeq_id::e_Other:
        seqId.SetOther(textId);
        break;
    case objects::CSeq_id::e_Ddbj:
        seqId.SetDdbj(textId);
        break;
    case objects::CSeq_id::e_Prf:
        seqId.SetPrf(textId);
        break;
    case objects::CSeq_id::e_Pdb:
    {
        // TODO: test this branch
        objects::CPDB_seq_id pdbId;
        pdbId.SetChain_id(0);
        seqId.SetPdb(pdbId);
    }
    break;
    case objects::CSeq_id::e_Tpg:
        seqId.SetTpg(textId);
        break;
    case objects::CSeq_id::e_Tpe:
        seqId.SetTpe(textId);
        break;
    case objects::CSeq_id::e_Tpd:
        seqId.SetTpd(textId);
        break;
    case objects::CSeq_id::e_Gpipe:
        seqId.SetGpipe(textId);
        break;
    case objects::CSeq_id::e_Named_annot_track:
        seqId.SetNamed_annot_track(textId);
        break;

    default:
        wasSet = false;
    }

    return wasSet;
}

/**********************************************************/
bool IsCancelled(const TKeywordList& keywords)
{
    ITERATE(TKeywordList, key, keywords)
    {
        if (NStr::EqualNocase(*key, "HTGS_CANCELLED"))
            return true;
    }

    return false;
}

/**********************************************************/
bool HasHtg(const TKeywordList& keywords)
{
    ITERATE(TKeywordList, key, keywords)
    {
        if (*key == "HTG" || *key == "HTGS_PHASE0" ||
            *key == "HTGS_PHASE1" || *key == "HTGS_PHASE2" ||
            *key == "HTGS_PHASE3")
        {
            return true;
        }
    }

    return false;
}

/**********************************************************/
void RemoveHtgPhase(TKeywordList& keywords)
{
    for (TKeywordList::iterator key = keywords.begin(); key != keywords.end();)
    {
        const char* p = key->c_str();
        if (NStr::EqualNocase(p, 0, 10, "HTGS_PHASE") &&
            (p[10] == '0' || p[10] == '1' || p[10] == '2' ||
            p[10] == '3') && p[11] == '\0')
        {
            key = keywords.erase(key);
        }
        else
            ++key;
    }
}

/**********************************************************/
bool HasHtc(const TKeywordList& keywords)
{
    ITERATE(TKeywordList, key, keywords)
    {
        if (NStr::EqualNocase(*key, "HTC"))
        {
            return true;
        }
    }

    return false;
}

END_NCBI_SCOPE
