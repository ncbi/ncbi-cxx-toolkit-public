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
 * File Name: utilfun.cpp
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *      Utility functions for parser and indexing.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>

#include "ftacpp.hpp"

#include <corelib/ncbistr.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <corelib/tempstr.hpp>

#include "index.h"

#include "ftaerr.hpp"
#include "indx_def.h"
#include "utilfun.h"
#include <algorithm>
#include <charconv>
#include <optional>

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "utilfun.cpp"

BEGIN_NCBI_SCOPE;

USING_SCOPE(objects);

CScope& GetScope()
{
    static CScope scope(*CObjectManager::GetInstance());
    return scope;
}


static const char* ParFlat_EST_kw_array[] = {
    "EST",
    "EST PROTO((expressed sequence tag)",
    "expressed sequence tag",
    "EST (expressed sequence tag)",
    "EST (expressed sequence tags)",
    "EST(expressed sequence tag)",
    "transcribed sequence fragment",
    nullptr
};

static const char* ParFlat_GSS_kw_array[] = {
    "GSS",
    "GSS (genome survey sequence)",
    "trapped exon",
    nullptr
};

static const char* ParFlat_STS_kw_array[] = {
    "STS",
    "STS(sequence tagged site)",
    "STS (sequence tagged site)",
    "STS sequence",
    "sequence tagged site",
    nullptr
};

static const char* ParFlat_HTC_kw_array[] = {
    "HTC",
    nullptr
};

static const char* ParFlat_FLI_kw_array[] = {
    "FLI_CDNA",
    nullptr
};

static const char* ParFlat_WGS_kw_array[] = {
    "WGS",
    nullptr
};

static const char* ParFlat_MGA_kw_array[] = {
    "MGA",
    "CAGE (Cap Analysis Gene Expression)",
    "5'-SAGE",
    nullptr
};

static const char* ParFlat_MGA_more_kw_array[] = {
    "CAGE (Cap Analysis Gene Expression)",
    "5'-SAGE",
    "5'-end tag",
    "unspecified tag",
    "small RNA",
    nullptr
};

/* Any change of contents of next array below requires proper
 * modifications in function fta_tsa_keywords_check().
 */
static const char* ParFlat_TSA_kw_array[] = {
    "TSA",
    "Transcriptome Shotgun Assembly",
    nullptr
};

/* Any change of contents of next array below requires proper
 * modifications in function fta_tls_keywords_check().
 */
static const char* ParFlat_TLS_kw_array[] = {
    "TLS",
    "Targeted Locus Study",
    nullptr
};

/* Any change of contents of next 2 arrays below requires proper
 * modifications in function fta_tpa_keywords_check().
 */
static const char* ParFlat_TPA_kw_array[] = {
    "TPA",
    "THIRD PARTY ANNOTATION",
    "THIRD PARTY DATA",
    "TPA:INFERENTIAL",
    "TPA:EXPERIMENTAL",
    "TPA:REASSEMBLY",
    "TPA:ASSEMBLY",
    "TPA:SPECIALIST_DB",
    nullptr
};

static const char* ParFlat_TPA_kw_array_to_remove[] = {
    "TPA",
    "THIRD PARTY ANNOTATION",
    "THIRD PARTY DATA",
    nullptr
};

static const char* ParFlat_ENV_kw_array[] = {
    "ENV",
    nullptr
};

static const char* ParFlat_MAG_kw_array[] = {
    "Metagenome Assembled Genome",
    "MAG",
    nullptr
};

/**********************************************************/
static string FTAitoa(Int4 m)
{
    Int4   sign = (m < 0) ? -1 : 1;
    string res;

    for (m *= sign; m > 9; m /= 10)
        res += m % 10 + '0';

    res += m + '0';

    if (sign < 0)
        res += '-';

    std::reverse(res.begin(), res.end());
    return res;
}

/**********************************************************/
void UnwrapAccessionRange(const CGB_block::TExtra_accessions& extra_accs, CGB_block::TExtra_accessions& hist)
{
    Int4 num1;
    Int4 num2;

    CGB_block::TExtra_accessions ret;

    for (const string& acc : extra_accs) {
        if (acc.empty())
            continue;

        size_t dash = acc.find('-');
        if (dash == string::npos) {
            ret.push_back(acc);
            continue;
        }

        string first(acc.begin(), acc.begin() + dash),
            last(acc.begin() + dash + 1, acc.end());
        size_t acclen = first.size();

        const Char* p = first.c_str();
        for (; (*p >= 'A' && *p <= 'Z') || *p == '_';)
            p++;

        size_t preflen = p - first.c_str();

        string prefix = first.substr(0, preflen);
        while (*p == '0')
            p++;

        const Char* q;
        for (q = p; *p >= '0' && *p <= '9';)
            p++;
        num1 = fta_atoi(q);

        for (p = last.c_str() + preflen; *p == '0';)
            p++;
        for (q = p; *p >= '0' && *p <= '9';)
            p++;
        num2 = fta_atoi(q);

        ret.push_back(first);

        if (num1 == num2)
            continue;

        for (num1++; num1 <= num2; num1++) {
            string new_acc = prefix;
            string num_str = FTAitoa(num1);
            size_t j       = acclen - preflen - num_str.size();

            for (size_t i = 0; i < j; i++)
                new_acc += '0';

            new_acc += num_str;
            ret.push_back(new_acc);
        }
    }

    ret.swap(hist);
}

static inline bool sIsPrefixChar(char c)
{
    return ('A' <= c && c <= 'Z') || c == '_';
}

inline bool IsLeadPrefixChar(char c)
{
    return ('A' <= c && c <= 'Z');
}
inline bool IsDigit(char c)
{
    return ('0' <= c && c <= '9');
}
/**********************************************************/
bool ParseAccessionRange(TokenStatBlk& tsbp, unsigned skip)
{
    auto& tokens = tsbp.list;
    if (tokens.empty())
        return true;
    if ((int)skip >= tsbp.num)
        return true;

    auto tbp = tokens.begin();
    if (skip > 0)
        advance(tbp, skip);

    bool bad = false, msg_issued = false;
    for (; tbp != tokens.end(); ++tbp) {
        const string& token    = *tbp;
        string_view   tok_view = token;
        if (token.empty())
            continue;
        size_t dash = token.find('-');
        if (dash == string::npos)
            continue;
        if (dash == 0 || tok_view.size() != (dash + 1 + dash)) {
            bad = true;
            break;
        }

        string_view first(tok_view.substr(0, dash));
        string_view last(tok_view.substr(dash + 1));
        if (! IsLeadPrefixChar(first.front()) || ! IsLeadPrefixChar(last.front())) {
            bad = true;
            break;
        }

        auto first_it = find_if_not(first.begin(), first.end(), sIsPrefixChar);
        if (first_it == first.end() || ! IsDigit(*first_it)) {
            bad = true;
            break;
        }
        auto last_it = find_if_not(last.begin(), last.end(), sIsPrefixChar);
        if (last_it == last.end() || ! IsDigit(*last_it)) {
            bad = true;
            break;
        }

        size_t      preflen      = first_it - first.begin();
        size_t      preflen2     = last_it - last.begin();
        string_view first_prefix = first.substr(0, preflen);
        string_view last_prefix  = last.substr(0, preflen2);
        if (first_prefix != last_prefix) {
            msg_issued = true;
            FtaErrPost(SEV_REJECT, ERR_ACCESSION_2ndAccPrefixMismatch, "Inconsistent prefix found in secondary accession range \"{}\".", token);
            bad = true;
            break;
        }

        string_view first_digits = first.substr(preflen);
        string_view last_digits  = last.substr(preflen);
        if (! all_of(first_digits.begin(), first_digits.end(), IsDigit) ||
            ! all_of(last_digits.begin(), last_digits.end(), IsDigit)) {
            bad = true;
            break;
        }

        auto num1 = NStr::StringToInt(first_digits, NStr::fConvErr_NoThrow);
        auto num2 = NStr::StringToInt(last_digits, NStr::fConvErr_NoThrow);
        if (num2 < num1) {
            msg_issued = true;
            FtaErrPost(SEV_REJECT, ERR_ACCESSION_Invalid2ndAccRange, "Invalid start/end values in secondary accession range \"{}\".", token);
            bad = true;
            break;
        }

        // cut in half
        string tmp(last);
        tbp->resize(dash);
        tbp = tokens.insert_after(tbp, "-");
        tbp = tokens.insert_after(tbp, tmp);
        tsbp.num += 2;
    }
    if (! bad)
        return true;
    if (! msg_issued) {
        FtaErrPost(SEV_REJECT, ERR_ACCESSION_Invalid2ndAccRange, "Incorrect secondary accession range provided: \"{}\".", tbp->c_str());
    }
    return false;
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
TokenStatBlk TokenString(string_view str, Char delimiter)
{
    TokenStatBlk tokens;
    auto tail = tokens.list.before_begin();

    /* skip first several delimiters if any existed
     */
    auto ptr = str.begin();
    while (ptr != str.end() && *ptr == delimiter)
        ptr++;

    while (ptr != str.end() && *ptr != '\r' && *ptr != '\n') {
        auto bptr = ptr;
        while (ptr != str.end() && *ptr != delimiter && *ptr != '\r' && *ptr != '\n' &&
               *ptr != '\t' && *ptr != ' ')
            ptr++;

        tail = tokens.list.insert_after(tail, string(bptr, ptr));
        tokens.num++;

        while (ptr != str.end() && (*ptr == delimiter || *ptr == '\t' || *ptr == ' '))
            ptr++;
    }

    return tokens;
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
Int2 fta_StringMatch(const Char** array, string_view text)
{
    Int2 i;

    for (i = 0; *array; i++, array++) {
        if (NStr::EqualCase(text, 0, StringLen(*array), *array))
            return i;
    }

    return -1;
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
Int2 StringMatchIcase(const Char** array, string_view text)
{
    Int2 i;

    for (i = 0; *array; i++, array++) {
        // If string from an array is empty its length == 0 and would be equval to any other string
        // The next 'if' statement will avoid that behavior
        if (! text.empty() && *array[0] == 0)
            continue;

        if (NStr::EqualNocase(text, 0, StringLen(*array), *array))
            return i;
    }

    return -1;
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
Int2 MatchArrayString(const char** array, string_view text)
{
    Int2 i;

    if (text.empty())
        return (-1);

    for (i = 0; *array; i++, array++) {
        if (NStr::Equal(*array, text))
            return i;
    }

    return -1;
}

/**********************************************************/
Int2 MatchArrayIString(const Char** array, string_view text)
{
    Int2 i;

    if (text.empty())
        return (-1);

    for (i = 0; *array; i++, array++) {
        if (NStr::EqualNocase(*array, text))
            return i;
    }

    return -1;
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
Int2 MatchArraySubString(const Char** array, string_view text)
{
    Int2 i;

    for (i = 0; *array; i++, array++) {
        if (NStr::Find(text, *array) != NPOS)
            return i;
    }

    return -1;
}

/**********************************************************/
Char* StringIStr(const Char* where, const Char* what)
{
    const Char* p;
    const Char* q;

    if (! where || *where == '\0' || ! what || *what == '\0')
        return nullptr;

    q = nullptr;
    for (; *where != '\0'; where++) {
        for (q = what, p = where; *q != '\0' && *p != '\0'; q++, p++) {
            if (*q == *p)
                continue;

            if (*q >= 'A' && *q <= 'Z') {
                if (*q + 32 == *p)
                    continue;
            } else if (*q >= 'a' && *q <= 'z') {
                if (*q - 32 == *p)
                    continue;
            }
            break;
        }
        if (*p == '\0' || *q == '\0')
            break;
    }
    if (q && *q == '\0')
        return const_cast<char*>(where);
    return nullptr;
}

/**********************************************************/
Int2 MatchArrayISubString(const Char** array, string_view text)
{
    Int2 i;

    for (i = 0; *array; i++, array++) {
        if (NStr::FindNoCase(text, *array) != NPOS)
            return i;
    }

    return -1;
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
string GetBlkDataReplaceNewLine(string_view instr, Uint2 indent)
{
    vector<string> lines;
    NStr::Split(instr, "\n", lines);
    string replaced;
    for (const auto& line : lines) {
        if (line.empty() || line.starts_with("XX") || line.size() <= indent) {
            continue;
        }
        replaced += line.substr(indent);
        auto last = line.size() - 1;
        if (line[last] != '-') {
            replaced += ' ';
        } else if (line[last - 1] == ' ') {
            replaced += ' ';
        }
    }
    NStr::TruncateSpacesInPlace(replaced);
    return replaced;
}


/**********************************************************/
static size_t SeekLastAlphaChar(string_view str)
{
    if (! str.empty()) {
        for (size_t ret = str.size(); ret > 0;) {
            char c = str[--ret];
            if (c != ' ' && c != '\n' && c != '\\' && c != ',' &&
                c != ';' && c != '~' && c != '.' && c != ':') {
                return ret + 1;
            }
        }
    }

    return 0;
}

/**********************************************************
 *
 *   void CleanTailNoneAlphaChar(str):
 *
 *      Delete any tailing ' ', '\n', '\\', ',', ';', '~',
 *   '.', ':' characters.
 *
 **********************************************************/
void CleanTailNonAlphaChar(string& str)
{
    size_t n = SeekLastAlphaChar(str);
    str.resize(n);
}

/**********************************************************/
void PointToNextToken(char*& ptr)
{
    if (ptr) {
        while (*ptr != ' ')
            ptr++;
        while (*ptr == ' ')
            ptr++;
    }
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
string GetTheCurrentToken(char** ptr)
{
    char* retptr;
    char* bptr;

    bptr = retptr = *ptr;
    if (! retptr || *retptr == '\0')
        return {};

    while (*retptr != '\0' && *retptr != ' ')
        retptr++;

    string str(bptr, retptr);

    while (*retptr != '\0' && *retptr == ' ') /* skip blanks */
        retptr++;
    *ptr = retptr;

    CleanTailNonAlphaChar(str);
    return (str);
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
char* SrchTheChar(string_view sv, Char letter)
{
    auto i = sv.find(letter);
    if (i != string_view::npos)
        return const_cast<char*>(sv.data() + i);
    else
        return nullptr;
}

/**********************************************************/
void CpSeqId(InfoBioseqPtr ibp, const CSeq_id& id)
{
    const CTextseq_id* text_id = id.GetTextseq_Id();
    if (text_id) {
        if (text_id->IsSetName())
            ibp->mLocus = text_id->GetName();

        CRef<CSeq_id> new_id(new CSeq_id);
        if (text_id->IsSetAccession()) {
            ibp->mAccNum = text_id->GetAccession();

            CRef<CTextseq_id> new_text_id(new CTextseq_id);
            new_text_id->SetAccession(text_id->GetAccession());
            if (text_id->IsSetVersion())
                new_text_id->SetVersion(text_id->GetVersion());

            SetTextId(id.Which(), *new_id, *new_text_id);
        } else {
            new_id->Assign(id);
        }

        ibp->ids.push_back(new_id);
    } else {
        auto pId = Ref(new CSeq_id());
        pId->Assign(id);
        ibp->ids.push_back(std::move(pId));
    }
}


static optional<int> s_GetNextInt(string_view sv) {

    int val{};
    if (from_chars(sv.data(), sv.data()+sv.size(), val).ec == errc{}) {
        return val;
    }

    return nullopt;
}

/**********************************************************
 *
 *   CRef<CDate_std> get_full_date(s, is_ref, source):
 *
 *      Get year, month, day and return CRef<CDate_std>.
 *
 **********************************************************/
CRef<CDate_std> get_full_date(string_view date_view, bool is_ref, Parser::ESource source)
{
    if (date_view.length()<3) {
        return {};
    }

    int day = 0;
    if (isdigit(date_view.front())) {
        day = s_GetNextInt(date_view).value();
        date_view = date_view.substr(3);
        // should we make at least a token effort of validation (like <32)?
    }

    static const vector<string> months{
        "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
    };
    string_view maybe_month = date_view.substr(0,3);
    auto        it = find(months.begin(), months.end(), maybe_month);

    if (it == months.end()) {
        string_view msg = date_view.substr(0,10); // This still works if date_view.length() < 10
        is_ref ? FtaErrPost(SEV_WARNING, ERR_REFERENCE_IllegalDate, "Unrecognized month: {}", msg)
               : FtaErrPost(SEV_WARNING, ERR_DATE_IllegalDate, "Unrecognized month: {}", msg);
        return {};
    }
    int month = int(it - months.begin()) + 1;

    date_view = date_view.substr(4);
    auto parsed_year = s_GetNextInt(date_view);
    if (! parsed_year.has_value()) {
        return {};
    }
    auto year = *parsed_year;


    int cur_year   = CCurrentTime().Year();
    if (1900 <= year && year <= cur_year) {
        // all set
    } else if (0 <= year && year <= 99 && '0' <= date_view[1] && date_view[1] <= '9') {
        // insist that short form year has exactly two digits
        (year < 70) ? (year += 2000) : (year += 1900);
    } else {
        if (is_ref) {
            FtaErrPost(SEV_ERROR, ERR_REFERENCE_IllegalDate, "Illegal year: {}, current year: {}", year, cur_year);
        } else if (source != Parser::ESource::SPROT || year - cur_year > 1) {
            FtaErrPost(SEV_WARNING, ERR_DATE_IllegalDate, "Illegal year: {}, current year: {}", year, cur_year);
        }
        // treat bad year like bad month above:
        return {};
    }

    auto date = Ref(new CDate_std());
    date->SetYear(year);
    date->SetMonth(month);
    date->SetDay(day);

    return date;
}

/**********************************************************
 *
 *   int SrchKeyword(ptr, kwl):
 *
 *      Compare first kwl.len byte in ptr to kwl.str.
 *      Return the position of keyword block array;
 *   return unknown keyword, UNKW, if not found.
 *
 *                                              3-25-93
 *
 **********************************************************/
int SrchKeyword(string_view str, const vector<string>& keywordList)
{
    SIZE_TYPE keywordCount = keywordList.size();

    for (unsigned i = 0; i < keywordCount; ++i) {
        if (str.starts_with(keywordList[i])) {
            return (int)i;
        }
    }
    return ParFlat_UNKW;
}

/**********************************************************/
bool CheckLineType(string_view str, Int4 type, const vector<string>& keywordList, bool after_origin)
{
    if (after_origin) {
        for (char c : str) {
            if (c >= '0' && c <= '9')
                continue;
            if (c == ' ')
                return true;
            break;
        }
    }

    for (const auto& keyword : keywordList) {
        if (str.starts_with(keyword))
            return true;
    }

    string_view msg(str);
    if (msg.size() > 50)
        msg = msg.substr(0, 50);
    auto n = msg.find('\n');
    if (n != string_view::npos)
        msg = msg.substr(0, n);
    FtaErrPost(SEV_ERROR, ERR_ENTRY_InvalidLineType, "Unknown linetype \"{}\". Line number {}.", msg, type);

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
bool SrchNodeType(const DataBlk& entry, Int4 type, size_t* plen, char** pptr)
{
    const DataBlk* temp = TrackNodeType(entry, (Int2)type);
    if (temp) {
        *plen = temp->mBuf.len;
        *pptr = temp->mBuf.ptr;
        return true;
    }

    *plen = 0;
    *pptr = nullptr;
    return false;
}

string_view GetNodeData(const DataBlk& entry, int nodeType)
{
    const DataBlk* tmp = TrackNodeType(entry, (Int2)nodeType);
    if (! tmp) {
        return {};
    }
    return string_view(tmp->mBuf.ptr, tmp->mBuf.len);
}

/**********************************************************
 *
 *   DataBlkPtr TrackNodeType(entry, type):
 *
 *      Return a pointer points to the Node which has
 *   the "type".
 *
 **********************************************************/
TDataBlkList& TrackNodes(const DataBlk& entry)
{
    EntryBlkPtr ebp = entry.GetEntryData();
    return ebp->chain;
}

DataBlk* TrackNodeType(const DataBlk& entry, Int2 type)
{
    auto& chain = TrackNodes(entry);
    for (auto& temp : chain)
        if (temp.mType == type)
            return &temp;

    return nullptr;
}


const Section* xTrackNodeType(const Entry& entry, int type)
{
    for (const Section* sectionPtr : entry.mSections) {
        if (sectionPtr->mType == type) {
            return sectionPtr;
        }
    }
    return nullptr;
}


/**********************************************************/
bool fta_tpa_keywords_check(const TKeywordList& kwds)
{
    const char* b[4];

    bool kwd_tpa   = false;
    bool kwd_party = false;
    bool kwd_inf   = false;
    bool kwd_exp   = false;
    bool kwd_asm   = false;
    bool kwd_spedb = false;
    bool ret       = true;

    Int4 j;
    Int2 i;

    if (kwds.empty())
        return true;

    size_t len = 0;
    j          = 0;
    for (const string& key : kwds) {
        if (key.empty())
            continue;

        const char* p = key.c_str();
        i             = MatchArrayIString(ParFlat_TPA_kw_array, key);
        if (i == 0)
            kwd_tpa = true;
        else if (i == 1 || i == 2)
            kwd_party = true;
        else if (i == 3)
            kwd_inf = true;
        else if (i == 4)
            kwd_exp = true;
        else if (i == 5 || i == 6)
            kwd_asm = true;
        else if (i == 7)
            kwd_spedb = true;
        else if (NStr::EqualNocase(p, 0, 3, "TPA")) {
            if (p[3] == ':') {
                FtaErrPost(SEV_REJECT, ERR_KEYWORD_InvalidTPATier, "Keyword \"{}\" is not a valid TPA-tier keyword.", p);
                ret = false;
            } else if (p[3] != '\0' && p[4] != '\0') {
                FtaErrPost(SEV_WARNING, ERR_KEYWORD_UnexpectedTPA, "Keyword \"{}\" looks like it might be TPA-related, but it is not a recognized TPA keyword.", p);
            }
        }
        if (i > 2 && i < 8 && j < 4) {
            b[j] = p;
            ++j;
            len += key.size() + 1;
        }
    }

    if (kwd_tpa && ! kwd_party) {
        FtaErrPost(SEV_REJECT, ERR_KEYWORD_MissingTPAKeywords, "This TPA-record should have keyword \"Third Party Annotation\" or \"Third Party Data\" in addition to \"TPA\".");
        ret = false;
    } else if (! kwd_tpa && kwd_party) {
        FtaErrPost(SEV_REJECT, ERR_KEYWORD_MissingTPAKeywords, "This TPA-record should have keyword \"TPA\" in addition to \"Third Party Annotation\" or \"Third Party Data\".");
        ret = false;
    }
    if (! kwd_tpa && (kwd_inf || kwd_exp)) {
        FtaErrPost(SEV_REJECT, ERR_KEYWORD_MissingTPAKeywords, "This TPA-record should have keyword \"TPA\" in addition to its TPA-tier keyword.");
        ret = false;
    } else if (kwd_tpa && kwd_inf == false && kwd_exp == false &&
               kwd_asm == false && kwd_spedb == false) {
        FtaErrPost(SEV_ERROR, ERR_KEYWORD_MissingTPATier, "This TPA record lacks a keyword to indicate which tier it belongs to: experimental, inferential, reassembly or specialist_db.");
    }
    if (j > 1) {
        string buf;
        for (i = 0; i < j; i++) {
            if (i > 0)
                buf += ';';
            buf += b[i];
        }
        FtaErrPost(SEV_REJECT, ERR_KEYWORD_ConflictingTPATiers, "Keywords for multiple TPA tiers exist on this record: \"{}\". A TPA record can only be in one tier.", buf);
        ret = false;
    }

    return (ret);
}

/**********************************************************/
bool fta_tsa_keywords_check(const TKeywordList& kwds, Parser::ESource source)
{
    bool kwd_tsa      = false;
    bool kwd_assembly = false;
    bool ret          = true;
    Int2 i;

    if (kwds.empty())
        return true;

    for (const string& key : kwds) {
        if (key.empty())
            continue;
        i = MatchArrayIString(ParFlat_TSA_kw_array, key);
        if (i == 0)
            kwd_tsa = true;
        else if (i == 1)
            kwd_assembly = true;
        else if (source == Parser::ESource::EMBL &&
                 NStr::EqualNocase(key, "Transcript Shotgun Assembly"))
            kwd_assembly = true;
    }

    if (kwd_tsa && ! kwd_assembly) {
        FtaErrPost(SEV_REJECT, ERR_KEYWORD_MissingTSAKeywords, "This TSA-record should have keyword \"Transcriptome Shotgun Assembly\" in addition to \"TSA\".");
        ret = false;
    } else if (! kwd_tsa && kwd_assembly) {
        FtaErrPost(SEV_REJECT, ERR_KEYWORD_MissingTSAKeywords, "This TSA-record should have keyword \"TSA\" in addition to \"Transcriptome Shotgun Assembly\".");
        ret = false;
    }
    return (ret);
}

/**********************************************************/
bool fta_tls_keywords_check(const TKeywordList& kwds, Parser::ESource source)
{
    bool kwd_tls   = false;
    bool kwd_study = false;
    bool ret       = true;
    Int2 i;

    if (kwds.empty())
        return true;

    for (const string& key : kwds) {
        if (key.empty())
            continue;
        i = MatchArrayIString(ParFlat_TLS_kw_array, key);
        if (i == 0)
            kwd_tls = true;
        else if (i == 1)
            kwd_study = true;
        else if (source == Parser::ESource::EMBL &&
                 NStr::EqualNocase(key, "Targeted Locus Study"))
            kwd_study = true;
    }

    if (kwd_tls && ! kwd_study) {
        FtaErrPost(SEV_REJECT, ERR_KEYWORD_MissingTLSKeywords, "This TLS-record should have keyword \"Targeted Locus Study\" in addition to \"TLS\".");
        ret = false;
    } else if (! kwd_tls && kwd_study) {
        FtaErrPost(SEV_REJECT, ERR_KEYWORD_MissingTLSKeywords, "This TLS-record should have keyword \"TLS\" in addition to \"Targeted Locus Study\".");
        ret = false;
    }
    return (ret);
}

/**********************************************************/
bool fta_is_tpa_keyword(string_view str)
{
    if (str.empty() || MatchArrayIString(ParFlat_TPA_kw_array, str) < 0)
        return false;

    return true;
}

/**********************************************************/
bool fta_is_tsa_keyword(string_view str)
{
    if (str.empty() || MatchArrayIString(ParFlat_TSA_kw_array, str) < 0)
        return false;
    return true;
}

/**********************************************************/
bool fta_is_tls_keyword(string_view str)
{
    if (str.empty() || MatchArrayIString(ParFlat_TLS_kw_array, str) < 0)
        return false;
    return true;
}

/**********************************************************/
void fta_keywords_check(string_view str, bool* estk, bool* stsk, bool* gssk, bool* htck, bool* flik, bool* wgsk, bool* tpak, bool* envk, bool* mgak, bool* tsak, bool* tlsk)
{
    if (estk && MatchArrayString(ParFlat_EST_kw_array, str) != -1)
        *estk = true;

    if (stsk && MatchArrayString(ParFlat_STS_kw_array, str) != -1)
        *stsk = true;

    if (gssk && MatchArrayString(ParFlat_GSS_kw_array, str) != -1)
        *gssk = true;

    if (htck && MatchArrayString(ParFlat_HTC_kw_array, str) != -1)
        *htck = true;

    if (flik && MatchArrayString(ParFlat_FLI_kw_array, str) != -1)
        *flik = true;

    if (wgsk && MatchArrayString(ParFlat_WGS_kw_array, str) != -1)
        *wgsk = true;

    if (tpak && MatchArrayString(ParFlat_TPA_kw_array, str) != -1)
        *tpak = true;

    if (envk && MatchArrayString(ParFlat_ENV_kw_array, str) != -1)
        *envk = true;

    if (mgak && MatchArrayString(ParFlat_MGA_kw_array, str) != -1)
        *mgak = true;

    if (tsak && MatchArrayString(ParFlat_TSA_kw_array, str) != -1)
        *tsak = true;

    if (tlsk && MatchArrayString(ParFlat_TLS_kw_array, str) != -1)
        *tlsk = true;
}

/**********************************************************/
void fta_remove_keywords(CMolInfo::TTech tech, TKeywordList& kwds)
{
    const char** b;

    if (kwds.empty())
        return;

    if (tech == CMolInfo::eTech_est)
        b = ParFlat_EST_kw_array;
    else if (tech == CMolInfo::eTech_sts)
        b = ParFlat_STS_kw_array;
    else if (tech == CMolInfo::eTech_survey)
        b = ParFlat_GSS_kw_array;
    else if (tech == CMolInfo::eTech_htc)
        b = ParFlat_HTC_kw_array;
    else if (tech == CMolInfo::eTech_fli_cdna)
        b = ParFlat_FLI_kw_array;
    else if (tech == CMolInfo::eTech_wgs)
        b = ParFlat_WGS_kw_array;
    else
        return;

    for (TKeywordList::iterator key = kwds.begin(); key != kwds.end();) {
        if (key->empty() || MatchArrayString(b, *key) != -1) {
            key = kwds.erase(key);
        } else
            ++key;
    }
}

/**********************************************************/
void fta_remove_tpa_keywords(TKeywordList& kwds)
{
    if (kwds.empty())
        return;

    for (TKeywordList::iterator key = kwds.begin(); key != kwds.end();) {
        if (key->empty() || MatchArrayIString(ParFlat_TPA_kw_array_to_remove, *key) != -1) {
            key = kwds.erase(key);
        } else
            ++key;
    }
}

/**********************************************************/
void fta_remove_tsa_keywords(TKeywordList& kwds, Parser::ESource source)
{
    if (kwds.empty())
        return;

    for (TKeywordList::iterator key = kwds.begin(); key != kwds.end();) {
        if (key->empty() || MatchArrayIString(ParFlat_TSA_kw_array, *key) != -1 ||
            (source == Parser::ESource::EMBL && NStr::EqualNocase(*key, "Transcript Shotgun Assembly"))) {
            key = kwds.erase(key);
        } else
            ++key;
    }
}

/**********************************************************/
void fta_remove_tls_keywords(TKeywordList& kwds, Parser::ESource source)
{
    if (kwds.empty())
        return;

    for (TKeywordList::iterator key = kwds.begin(); key != kwds.end();) {
        if (key->empty() || MatchArrayIString(ParFlat_TLS_kw_array, *key) != -1 ||
            (source == Parser::ESource::EMBL && NStr::EqualNocase(*key, "Targeted Locus Study"))) {
            key = kwds.erase(key);
        } else
            ++key;
    }
}

/**********************************************************/
void fta_remove_env_keywords(TKeywordList& kwds)
{
    if (kwds.empty())
        return;

    for (TKeywordList::iterator key = kwds.begin(); key != kwds.end();) {
        if (key->empty() || MatchArrayIString(ParFlat_ENV_kw_array, *key) != -1) {
            key = kwds.erase(key);
        } else
            ++key;
    }
}

/**********************************************************/
void fta_remove_mag_keywords(TKeywordList& kwds)
{
    if (kwds.empty())
        return;

    for (TKeywordList::iterator key = kwds.begin(); key != kwds.end();) {
        if (key->empty() || MatchArrayIString(ParFlat_MAG_kw_array, *key) != -1) {
            key = kwds.erase(key);
        } else
            ++key;
    }
}

/**********************************************************/
void xCheckEstStsGssTpaKeywords(
    const list<string> keywordList,
    bool               tpa_check,
    IndexblkPtr        entry
    // bool& specialist_db,
    // bool& inferential,
    // bool& experimental,
    // bool& assembly
)
{
    if (keywordList.empty()) {
        return;
    }
    for (auto keyword : keywordList) {
        fta_keywords_check(
            keyword, &entry->EST, &entry->STS, &entry->GSS, &entry->HTC, nullptr, nullptr, (tpa_check ? &entry->is_tpa : nullptr), nullptr, nullptr, nullptr, nullptr);
        if (NStr::EqualNocase(keyword, "TPA:assembly")) {
            entry->specialist_db = true;
            entry->assembly      = true;
            continue;
        }
        if (NStr::EqualNocase(keyword, "TPA:specialist_db")) {
            entry->specialist_db = true;
            continue;
        }
        if (NStr::EqualNocase(keyword, "TPA:inferential")) {
            entry->inferential = true;
            continue;
        }
        if (NStr::EqualNocase(keyword, "TPA:experimental")) {
            entry->experimental = true;
            continue;
        }
    }
}

void check_est_sts_gss_tpa_kwds(const TKeywordList& kwds, size_t len, IndexblkPtr entry, bool tpa_check, bool& specialist_db, bool& inferential, bool& experimental, bool& assembly)
{
    char* p;
    char* q;

    if (kwds.empty() || kwds.front().empty() || len < 1)
        return;

    string buf;
    buf.reserve(len);
    for (const auto& kwd : kwds) {
        buf += kwd;
    }
    char* line = buf.data();
    for (p = line; *p != '\0'; p++)
        if (*p == '\n' || *p == '\t')
            *p = ' ';
    for (p = line; *p == ' ' || *p == '.' || *p == ';';)
        p++;
    if (*p == '\0') {
        return;
    }
    for (q = p; *q != '\0';)
        q++;
    for (q--; *q == ' ' || *q == '.' || *q == ';'; q--)
        *q = '\0';
    for (q = p, p = line; *q != '\0';) {
        if (*q != ' ' && *q != ';') {
            *p++ = *q++;
            continue;
        }
        if (*q == ' ') {
            for (q++; *q == ' ';)
                q++;
            if (*q != ';')
                *p++ = ' ';
        }
        if (*q == ';') {
            *p++ = *q++;
            while (*q == ' ' || *q == ';')
                q++;
        }
    }
    *p++ = ';';
    *p   = '\0';
    for (p = line;; p = q + 1) {
        q = StringChr(p, ';');
        if (! q)
            break;
        *q = '\0';
        fta_keywords_check(p, &entry->EST, &entry->STS, &entry->GSS, &entry->HTC, nullptr, nullptr, (tpa_check ? &entry->is_tpa : nullptr), nullptr, nullptr, nullptr, nullptr);
        if (NStr::EqualNocase(p, "TPA:specialist_db") ||
            NStr::EqualNocase(p, "TPA:assembly")) {
            specialist_db = true;
            if (NStr::EqualNocase(p, "TPA:assembly"))
                assembly = true;
        } else if (NStr::EqualNocase(p, "TPA:inferential"))
            inferential = true;
        else if (NStr::EqualNocase(p, "TPA:experimental"))
            experimental = true;
    }
}

/**********************************************************/
bool fta_check_mga_keywords(CMolInfo& mol_info, const TKeywordList& kwds)
{
    bool is_cage;
    bool is_sage;

    TKeywordList::const_iterator key_it = kwds.end();

    bool got = false;
    if (! kwds.empty() && NStr::EqualNocase(kwds.front(), "MGA")) {
        for (TKeywordList::const_iterator key = kwds.begin(); key != kwds.end(); ++key) {
            if (MatchArrayIString(ParFlat_MGA_more_kw_array, *key) < 0)
                continue;
            got    = true;
            key_it = key;
            break;
        }
    }

    if (! got) {
        FtaErrPost(SEV_REJECT, ERR_KEYWORD_MissingMGAKeywords, "This is apparently a CAGE record, but it lacks the required keywords. Entry dropped.");
        return false;
    }

    if (! mol_info.IsSetTechexp() || ! kwds.empty() ||
        mol_info.GetTechexp() != "cage")
        return true;

    for (is_sage = false, is_cage = false; key_it != kwds.end(); ++key_it) {
        const char* p = key_it->c_str();

        if (NStr::EqualNocase(p, "5'-SAGE"))
            is_sage = true;
        else if (NStr::EqualNocase(p, "CAGE (Cap Analysis Gene Expression)"))
            is_cage = true;
    }

    if (is_sage) {
        if (is_cage) {
            FtaErrPost(SEV_REJECT, ERR_KEYWORD_ConflictingMGAKeywords, "This MGA record contains more than one of the special keywords indicating different techniques.");
            return false;
        }
        mol_info.SetTechexp("5'-sage");
    }

    return true;
}

/**********************************************************/
void fta_StringCpy(char* dst, const char* src)
{
    const char* p;
    char*       q;

    for (q = dst, p = src; *p != '\0';)
        *q++ = *p++;
    *q = '\0';
}

/**********************************************************/
bool SetTextId(Uint1 seqtype, CSeq_id& seqId, CTextseq_id& textId)
{
    bool wasSet = true;

    switch (seqtype) {
    case CSeq_id::e_Genbank:
        seqId.SetGenbank(textId);
        break;
    case CSeq_id::e_Embl:
        seqId.SetEmbl(textId);
        break;
    case CSeq_id::e_Pir:
        seqId.SetPir(textId);
        break;
    case CSeq_id::e_Swissprot:
        seqId.SetSwissprot(textId);
        break;
    case CSeq_id::e_Other:
        seqId.SetOther(textId);
        break;
    case CSeq_id::e_Ddbj:
        seqId.SetDdbj(textId);
        break;
    case CSeq_id::e_Prf:
        seqId.SetPrf(textId);
        break;
    case CSeq_id::e_Pdb: {
        // TODO: test this branch
        CPDB_seq_id pdbId;
        pdbId.SetChain_id();
        seqId.SetPdb(pdbId);
    } break;
    case CSeq_id::e_Tpg:
        seqId.SetTpg(textId);
        break;
    case CSeq_id::e_Tpe:
        seqId.SetTpe(textId);
        break;
    case CSeq_id::e_Tpd:
        seqId.SetTpd(textId);
        break;
    case CSeq_id::e_Gpipe:
        seqId.SetGpipe(textId);
        break;
    case CSeq_id::e_Named_annot_track:
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
    for (const string& key : keywords) {
        if (NStr::EqualNocase(key, "HTGS_CANCELLED"))
            return true;
    }

    return false;
}

/**********************************************************/
bool HasHtg(const TKeywordList& keywords)
{
    for (const string& key : keywords) {
        if (key == "HTG" || key == "HTGS_PHASE0" ||
            key == "HTGS_PHASE1" || key == "HTGS_PHASE2" ||
            key == "HTGS_PHASE3") {
            return true;
        }
    }

    return false;
}

/**********************************************************/
void RemoveHtgPhase(TKeywordList& keywords)
{
    for (TKeywordList::iterator key = keywords.begin(); key != keywords.end();) {
        const char* p = key->c_str();
        if (NStr::EqualNocase(p, 0, 10, "HTGS_PHASE") &&
            (p[10] == '0' || p[10] == '1' || p[10] == '2' ||
             p[10] == '3') &&
            p[11] == '\0') {
            key = keywords.erase(key);
        } else
            ++key;
    }
}

/**********************************************************/
bool HasHtc(const TKeywordList& keywords)
{
    for (const string& key : keywords) {
        if (NStr::EqualNocase(key, "HTC")) {
            return true;
        }
    }

    return false;
}

END_NCBI_SCOPE
