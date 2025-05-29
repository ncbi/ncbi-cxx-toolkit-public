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
 * File Name: xgbparint.cpp
 *
 * Author:  Alexey Dobronadezhdin (translated from gbparint.c made by Karl Sirotkin)
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbimisc.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include "ftacpp.hpp"
#include "ftaerr.hpp"
#include "xgbparint.h"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "xgbparint.cpp"

#define TAKE_FIRST  1
#define TAKE_SECOND 2

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

const char* seqlitdbtag    = "SeqLit";
const char* unkseqlitdbtag = "UnkSeqLit";

enum class ETokenType {
    eUnknown,
    eJoin,
    eCompl,
    eLeft,
    eRight,
    eCaret,
    eDotDot,
    eAccession,
    eGt,
    eLt,
    eComma,
    eNumber,
    eOrder,
    eSingleDot,
    eGroup,
    eOneOf,
    eReplace,
    eSites,
    eString,
    eOneOfNum,
    eGap,
    eUnkGap
};

struct STokenInfo {
    ETokenType choice;
    string     data;
};

using TTokens       = list<STokenInfo>;
using TTokenIt      = TTokens::iterator;
using TTokenConstIt = TTokens::const_iterator;


/*--------- do_xgbparse_error () ---------------*/

#define ERR_FEATURE_LocationParsing_validatr 1, 5

static void do_xgbparse_error(string_view msg, string_view details)
{
    string errmsg = string(msg) + " at " + string(details);
    Nlm_ErrSetContext("validatr", __FILE__, __LINE__);
    Nlm_ErrPostStr(SEV_ERROR, ERR_FEATURE_LocationParsing_validatr, errmsg);
}

static X_gbparse_errfunc   Err_func            = do_xgbparse_error;
static X_gbparse_rangefunc Range_func          = nullptr;
static void*               xgbparse_range_data = nullptr;

/*----------- xinstall_gbparse_error_handler ()-------------*/

void xinstall_gbparse_error_handler(X_gbparse_errfunc new_func)
{
    Err_func = new_func;
}

/*----------- xinstall_gbparse_range_func ()-------------*/

void xinstall_gbparse_range_func(void* data, X_gbparse_rangefunc new_func)
{
    Range_func          = new_func;
    xgbparse_range_data = data;
}

/*------ xgbparse_point ()----*/
static string xgb_unparse(TTokenConstIt head, TTokenConstIt end_it)
{
    string temp;
    for (auto it = head; it != end_it; ++it) {
        switch (it->choice) {
        case ETokenType::eJoin:
            temp += "join";
            break;
        case ETokenType::eCompl:
            temp += "complement";
            break;
        case ETokenType::eLeft:
            temp += "(";
            break;
        case ETokenType::eRight:
            temp += ")";
            break;
        case ETokenType::eCaret:
            temp += "^";
            break;
        case ETokenType::eDotDot:
            temp += "..";
            break;
        case ETokenType::eAccession:
            temp += it->data;
            temp += ":";
            break;
        case ETokenType::eNumber:
        case ETokenType::eString:
            temp += it->data;
            break;
        case ETokenType::eGt:
            temp += ">";
            break;
        case ETokenType::eLt:
            temp += "<";
            break;
        case ETokenType::eComma:
            temp += ",";
            break;
        case ETokenType::eOrder:
            temp += "order";
            break;
        case ETokenType::eSingleDot:
            temp += ".";
            break;
        case ETokenType::eGroup:
            temp += "group";
            break;
        case ETokenType::eOneOf:
        case ETokenType::eOneOfNum:
            temp += "one-of";
            break;
        case ETokenType::eReplace:
            temp += "replace";
            break;
        case ETokenType::eGap:
            temp += "gap";
            break;
        case ETokenType::eUnknown:
        default:
            break;
        }
    }

    return temp;
}


static void xgbparse_error(string_view front, TTokenConstIt head, TTokenConstIt end_it)
{
    string details = xgb_unparse(head, end_it);
    Err_func(front, details);
}


static void xgbparse_error(string_view front, const TTokens& tokens, TTokenConstIt current)
{
    if (current != end(tokens))
        ++current;
    xgbparse_error(front, begin(tokens), current);
}


/*------------------ xgbcheck_range()-------------*/
static void xgbcheck_range(TSeqPos num, const CSeq_id& id, bool& keep_rawPt, int& numErrors, const TTokens& tokens, TTokenConstIt current)
{
    if (! Range_func) {
        return;
    }

    const auto len = (*Range_func)(xgbparse_range_data, id);
    if (len != -1 && num >= (Uint4)len) {
        xgbparse_error("range error", tokens, current);
        keep_rawPt = true;
        ++numErrors;
    }
}


/*--------- xfind_one_of_num()------------*/
/*
Consider these for locations:
         misc_signal     join(57..one-of(67,75),one-of(100,110)..200)
     misc_signal     join(57..one-of(67,75),one-of(100,110..120),200)
     misc_signal     join(57..one-of(67,75),one-of(100,110..115)..200)

     misc_signal     join(57..one-of(67,75),one-of(100,110),200)

In the first three, the one-of() is functioning as an alternative set
of numbers, in the last, as an alternative set of locations (even
though the locations are points).
[yes the one-of(100,110..115).. is illegal]

  here is one more case:one-of(18,30)..470 so if the location
  starts with a one-of, it also needs to be checked.

To deal with this, the ETokenType::eOneOf token type will be changed
by the following function to ETokenType::eOneOfNum, in the three cases.

note that this change is not necessary in this case:
        join(100..200,300..one-of(400,500)), as after a ".." token,
    it has to be a number.

*/

static void xfind_one_of_num(list<STokenInfo>& tokens)
{
    if (tokens.size() == 1) {
        return;
    }
    auto current_it = begin(tokens);
    if (current_it->choice == ETokenType::eOneOf) {
        for (auto scanner_it = next(current_it);
             scanner_it != end(tokens);
             ++scanner_it) {
            if (scanner_it->choice == ETokenType::eRight) {
                ++scanner_it;
                if (scanner_it != end(tokens) &&
                    scanner_it->choice == ETokenType::eDotDot) {
                    current_it->choice = ETokenType::eOneOfNum;
                }
                break;
            }
        }
    }

    for (auto current_it = begin(tokens);
         current_it != end(tokens);
         ++current_it) {
        if (current_it->choice == ETokenType::eComma ||
            current_it->choice == ETokenType::eLeft) {
            auto scanner_it = next(current_it);
            if (scanner_it != end(tokens)) {
                if (scanner_it->choice == ETokenType::eOneOf) {
                    ++scanner_it;
                    while (scanner_it != end(tokens)) {
                        if (scanner_it->choice == ETokenType::eRight) {
                            ++scanner_it;
                            if (scanner_it != end(tokens) &&
                                scanner_it->choice == ETokenType::eDotDot) {
                                current_it->choice = ETokenType::eOneOfNum;
                            }
                            break;
                        }
                        ++scanner_it;
                    }
                }
            }
        }
    }
}


static void xlex_error_func(
    string_view   msg,
    const string& line,
    unsigned int  current_col)
{
    string temp_string = line.substr(0, current_col + 1) + " ";
    Err_func(msg, temp_string);
}


static unsigned advance_to(const char c, unsigned current_pos, const string& line)
{
    int pos = current_pos;
    while (pos < line.size()) {
        if (line[pos] == c) {
            return pos - 1;
        }
        ++pos;
    }
    return pos - 1;
}


static size_t sParseAccessionPrefix(string_view accession)
{
    if (accession.empty()) {
        return 0;
    }

    auto IsAlpha = [](char c) { return isalpha(c); };

    auto it = find_if_not(begin(accession),
                          end(accession),
                          IsAlpha);


    if (it == end(accession)) {
        return 1;
    }

    auto prefix_length = distance(begin(accession), it);
    if (*it == '_') {
        if (prefix_length != 2) {
            return 1;
        }
        ++it;
        it = find_if_not(it, end(accession), IsAlpha);
        if (it == end(accession)) {
            return 1;
        }
        prefix_length = distance(begin(accession), it);
        if (prefix_length == 3 || prefix_length == 7) {
            return prefix_length;
        }
        return 1;
    } else if (accession.size() >= 3 &&
               isdigit(accession[0]) &&
               isdigit(accession[1]) &&
               accession[2] == 'S') {
        return 7;
    }

    if (prefix_length == 1 ||
        prefix_length == 2 ||
        prefix_length == 4 ||
        prefix_length == 6) {
        return prefix_length;
    }

    return 1;
}


static int sGetAccession(string& accession, unsigned int& current_col, const string& line, bool accver)
{
    const auto  length = line.size();
    string_view tempString(line.c_str() + current_col, length - current_col);
    auto        prefixLength    = sParseAccessionPrefix(tempString);
    size_t      accessionLength = prefixLength;

    tempString       = tempString.substr(prefixLength);
    auto notDigitPos = tempString.find_first_not_of("0123456789");
    if (notDigitPos != string_view::npos) {
        accessionLength += notDigitPos;
        if (accver && tempString[notDigitPos] == '.') {
            ++accessionLength;
            if (tempString.size() > notDigitPos) {
                tempString  = tempString.substr(notDigitPos + 1);
                notDigitPos = tempString.find_first_not_of("0123456789");
                if (notDigitPos != string_view::npos) {
                    accessionLength += notDigitPos;
                }
            }
        }
    } else {
        accessionLength = length - current_col;
    }

    int retval = 0;
    if (notDigitPos == string_view::npos || tempString[notDigitPos] != ':') {
        xlex_error_func("ACCESSION missing \":\"", line, current_col);
        ++retval;
        --current_col;
    }

    accession = string(line.c_str() + current_col, accessionLength);
    current_col += accessionLength;
    return retval;
}


/*------------- xgbparselex_ver() -----------------------*/

static int xgbparselex_ver(string_view linein, TTokens& tokens, bool accver)
{
    tokens.clear();
    int retval = 0;
    if (! linein.empty()) {
        string line{ linein };
        NStr::TruncateSpacesInPlace(line);
        auto     length      = line.size();
        unsigned current_col = 0;

        while (current_col < length) {

            if (isspace(line[current_col]) || line[current_col] == '~') {
                ++current_col;
                continue;
            }

            STokenInfo current_token;
            if (isdigit(line[current_col])) {
                current_token.choice = ETokenType::eNumber;
                string_view tempString(line.c_str() + current_col, size_t(length - current_col));
                auto        not_digit_pos = tempString.find_first_not_of("0123456789");
                auto        num_digits    = (not_digit_pos == string_view::npos) ? size_t(length - current_col) : not_digit_pos;
                current_token.data        = string(line.c_str() + current_col, num_digits);
                tokens.push_back(current_token);
                current_col += num_digits;
                continue;
            }

            bool skip_new_token = false;
            switch (line[current_col]) {
            case '\"':
                current_token.choice = ETokenType::eString;
                if (auto closing_quote_pos = line.find('\"', current_col + 1);
                    closing_quote_pos == string::npos) {
                    xlex_error_func("unterminated string", line, current_col);
                    retval++;
                } else {
                    size_t len         = closing_quote_pos - current_col + 1;
                    current_token.data = string(line.c_str() + current_col, len);
                    current_col += len;
                }
                break;
                /*------
                 *  JOIN
                 *------*/
            case 'j':
                current_token.choice = ETokenType::eJoin;
                if (! NStr::StartsWith(line.c_str() + current_col, "join")) {
                    xlex_error_func("\"join\" misspelled", line, current_col);
                    retval++;
                    current_col = advance_to('(', current_col, line);
                } else {
                    current_col += 3;
                }
                break;

                /*------
                 *  ORDER and ONE-OF
                 *------*/
            case 'o':
                if (! NStr::StartsWith(line.c_str() + current_col, "order")) {
                    if (! NStr::StartsWith(line.c_str() + current_col, "one-of")) {
                        xlex_error_func("\"order\" or \"one-of\" misspelled",
                                        line,
                                        current_col);
                        retval++;
                        current_col = advance_to('(', current_col, line);
                    } else {
                        current_token.choice = ETokenType::eOneOf;
                        current_col += 5;
                    }
                } else {
                    current_token.choice = ETokenType::eOrder;
                    current_col += 4;
                }
                break;

                /*------
                 *  REPLACE
                 *------*/
            case 'r':
                current_token.choice = ETokenType::eReplace;
                if (! NStr::StartsWith(line.c_str() + current_col, "replace")) {
                    xlex_error_func("\"replace\" misspelled", line, current_col);
                    retval++;
                    current_col = advance_to('(', current_col, line);
                } else {
                    current_col += 6;
                }
                break;

                /*------
                 *  GAP or GROUP or GI
                 *------*/
            case 'g':
                if (NStr::StartsWith(line.c_str() + current_col, "gap") &&
                    (current_col < length - 3) &&
                    (line[current_col + 3] == '(' ||
                     line[current_col + 3] == ' ' ||
                     line[current_col + 3] == '\t' ||
                     line[current_col + 3] == '\0')) {
                    current_token.choice = ETokenType::eGap;
                    current_token.data   = "gap";
                    if (NStr::StartsWith(line.c_str() + current_col + 3, "(unk", NStr::eNocase)) {
                        current_token.choice = ETokenType::eUnkGap;
                        tokens.push_back(current_token);

                        current_token.choice = ETokenType::eLeft;
                        current_col += 4;
                    }
                    current_col += 2;
                    break;
                }
                if (NStr::StartsWith(line.c_str() + current_col, "gi|")) {
                    current_token.choice = ETokenType::eAccession;
                    current_col += 3;
                    for (; isdigit(line[current_col]); current_col++)
                        ;
                    break;
                }
                current_token.choice = ETokenType::eGroup;
                if (! NStr::StartsWith(line.c_str() + current_col, "group")) {
                    xlex_error_func("\"group\" misspelled", line, current_col);
                    retval++;
                    current_col = advance_to('(', current_col, line);
                } else {
                    current_col += 4;
                }
                break;

                /*------
                 *  COMPLEMENT
                 *------*/
            case 'c':
                current_token.choice = ETokenType::eCompl;
                if (! NStr::StartsWith(line.c_str() + current_col, "complement")) {
                    xlex_error_func("\"complement\" misspelled", line, current_col);
                    ++retval;
                    current_col = advance_to('(', current_col, line);
                } else {
                    current_col += 9;
                }
                break;

                /*-------
                 * internal bases ignored
                 *---------*/
            case 'b':
                if (! NStr::StartsWith(line.c_str() + current_col, "bases")) {
                    current_token.choice = ETokenType::eAccession;
                    retval += sGetAccession(current_token.data, current_col, line, accver);
                } else {
                    skip_new_token = true;
                    current_col += 4;
                }
                break;

                /*------
                 *  ()^.,<>  (bases (sites
                 *------*/
            case '(':
                if (NStr::StartsWith(line.c_str() + current_col, "(base")) {
                    current_token.choice = ETokenType::eJoin;
                    current_col += 4;
                    if (current_col < length - 1 && line[current_col + 1] == 's') {
                        current_col++;
                    }
                    tokens.push_back(current_token);
                    current_token.choice = ETokenType::eLeft;
                } else if (NStr::StartsWith(line.c_str() + current_col, "(sites")) {
                    current_col += 5;
                    if (current_col < length - 1) {
                        if (line[current_col + 1] == ')') {
                            current_col++;
                            current_token.choice = ETokenType::eSites;
                        } else {
                            current_token.choice = ETokenType::eSites;
                            tokens.push_back(current_token);
                            current_token.choice = ETokenType::eJoin;
                            tokens.push_back(current_token);
                            current_token.choice = ETokenType::eLeft;
                            if (current_col < length - 1) {
                                if (line[current_col + 1] == ';') {
                                    current_col++;
                                } else if (NStr::StartsWith(line.c_str() + current_col + 1, " ;")) {
                                    current_col += 2;
                                }
                            }
                        }
                    }
                } else {
                    current_token.choice = ETokenType::eLeft;
                }
                break;

            case ')':
                current_token.choice = ETokenType::eRight;
                break;

            case '^':
                current_token.choice = ETokenType::eCaret;
                break;

            case '-':
                current_token.choice = ETokenType::eDotDot;
                break;
            case '.':
                if (current_col == length - 1 || line[current_col + 1] != '.') {
                    current_token.choice = ETokenType::eSingleDot;
                } else {
                    current_token.choice = ETokenType::eDotDot;
                    current_col++;
                }
                break;

            case '>':
                current_token.choice = ETokenType::eGt;
                break;

            case '<':
                current_token.choice = ETokenType::eLt;
                break;

            case ';':
            case ',':
                current_token.choice = ETokenType::eComma;
                break;

            case 't':
                if (! NStr::StartsWith(line.c_str() + current_col, "to")) {
                    current_token.choice = ETokenType::eAccession;
                    retval += sGetAccession(current_token.data, current_col, line, accver);
                } else {
                    current_token.choice = ETokenType::eDotDot;
                    current_col++;
                }
                break;

            case 's':
                if (! NStr::StartsWith(line.c_str() + current_col, "site")) {
                    current_token.choice = ETokenType::eAccession;
                    retval += sGetAccession(current_token.data, current_col, line, accver);
                } else {
                    current_token.choice = ETokenType::eSites;
                    current_col += 3;
                    if (current_col < length - 1 && line[current_col + 1] == 's') {
                        current_col++;
                    }
                    if (current_col < length - 1) {
                        if (line[current_col + 1] == ';') {
                            current_col++;
                        } else if (NStr::StartsWith(line.c_str() + current_col + 1, " ;")) {
                            current_col += 2;
                        }
                    }
                }
                break;

            default:
                current_token.choice = ETokenType::eAccession;
                retval += sGetAccession(current_token.data, current_col, line, accver);
            }


            /*--move to past last "good" character---*/
            ++current_col;
            if (! skip_new_token) {
                tokens.push_back(current_token);
            }
        }
    }

    return retval;
}


/*----------------- xgbparse_better_be_done()-------------*/
static void xgbparse_better_be_done(int& numErrors, TTokenIt current_token, const TTokens& tokens, bool& keep_rawPt, int paren_count)
{
    if (current_token != end(tokens)) {
        while (current_token->choice == ETokenType::eRight) {
            paren_count--;
            ++current_token;
            if (current_token == end(tokens)) {
                if (paren_count) {
                    const string par_msg = "mismatched parentheses (" + to_string(paren_count) + ")";
                    xgbparse_error(par_msg, tokens, current_token);
                    keep_rawPt = true;
                    ++numErrors;
                }
                break;
            }
        }
    }

    if (paren_count) {
        xgbparse_error("text after last legal right parenthesis",
                       tokens,
                       current_token);
        keep_rawPt = true;
        ++numErrors;
    }

    if (current_token != end(tokens)) {
        xgbparse_error("text after end",
                       tokens,
                       current_token);
        keep_rawPt = true;
        ++numErrors;
    }
}


/**********************************************************
 *
 *   CRef<CSeq_loc> XGapToSeqLocEx(range, unknown):
 *
 *      Gets the size of gap and constructs SeqLoc block with
 *   $(seqlitdbtag) value as Dbtag.db and Dbtag.tag.id = 0.
 *
 **********************************************************/
static CRef<CSeq_loc> XGapToSeqLocEx(Int4 range, bool unknown)
{
    CRef<CSeq_loc> ret;

    if (range < 0)
        return ret;

    ret.Reset(new CSeq_loc);
    if (range == 0) {
        ret->SetNull();
        return ret;
    }

    CSeq_interval& interval = ret->SetInt();
    interval.SetFrom(0);
    interval.SetTo(range - 1);

    CSeq_id& id = interval.SetId();
    id.SetGeneral().SetDb(unknown ? unkseqlitdbtag : seqlitdbtag);
    id.SetGeneral().SetTag().SetId(0);

    return ret;
}

/**********************************************************/
static void xgbgap(TTokenIt& current_it, TTokenConstIt end_it, CRef<CSeq_loc>& loc, bool unknown)
{
    auto it = next(current_it);

    if (distance(TTokenConstIt(it), end_it) < 2) {
        return;
    }

    if (it->choice != ETokenType::eLeft) {
        return;
    }
    ++it;
    if (it->choice != ETokenType::eNumber &&
        it->choice != ETokenType::eRight) {
        return;
    }

    if (it->choice == ETokenType::eRight) {
        loc->SetNull();
    } else {
        auto gapsize_it = it++;
        if (it == end_it || it->choice != ETokenType::eRight) {
            return;
        }
        auto pLoc = XGapToSeqLocEx(fta_atoi(gapsize_it->data.c_str()), unknown);
        if (! pLoc) {
            return;
        }
        loc = pLoc;
    }
    current_it = next(it);
}

/*------------------- sConvertIntToPoint()-----------*/

static void sConvertIntToPoint(CSeq_loc& loc)
{
    CRef<CSeq_point> point(new CSeq_point);

    point->SetPoint(loc.GetInt().GetFrom());

    if (loc.GetInt().IsSetId())
        point->SetId(loc.SetInt().SetId());

    if (loc.GetInt().IsSetFuzz_from())
        point->SetFuzz(loc.SetInt().SetFuzz_from());

    loc.SetPnt(*point);
}


static void xgbload_number(TSeqPos& numPt, CInt_fuzz& fuzz, bool& keep_rawPt, TTokenIt& currentPt, const TTokens& tokens, int& numErrors, int take_which)
{
    int  num_found       = 0;
    int  fuzz_err        = 0;
    bool strange_sin_dot = false;
    auto end_it          = end(tokens);

    if (currentPt->choice == ETokenType::eCaret) {
        xgbparse_error("duplicate carets", tokens, currentPt);
        keep_rawPt = true;
        ++numErrors;
        ++currentPt;
        fuzz_err = 1;
    } else if (currentPt->choice == ETokenType::eGt ||
               currentPt->choice == ETokenType::eLt) {
        if (currentPt->choice == ETokenType::eGt)
            fuzz.SetLim(CInt_fuzz::eLim_gt);
        else
            fuzz.SetLim(CInt_fuzz::eLim_lt);

        ++currentPt;
    } else if (currentPt->choice == ETokenType::eLeft) {
        strange_sin_dot = true;
        ++currentPt;
        fuzz.SetRange();

        if (currentPt->choice == ETokenType::eNumber) {
            fuzz.SetRange().SetMin(fta_atoi(currentPt->data.c_str()) - 1);
            if (take_which == TAKE_FIRST) {
                numPt = fuzz.GetRange().GetMin();
            }
            ++currentPt;
            num_found = 1;
        } else
            fuzz_err = 1;

        if (currentPt->choice != ETokenType::eSingleDot)
            fuzz_err = 1;
        else {
            ++currentPt;
            if (currentPt->choice == ETokenType::eNumber) {
                fuzz.SetRange().SetMax(fta_atoi(currentPt->data.c_str()) - 1);
                if (take_which == TAKE_SECOND) {
                    numPt = fuzz.GetRange().GetMax();
                }
                ++currentPt;
            } else
                fuzz_err = 1;

            if (currentPt->choice == ETokenType::eRight)
                ++currentPt;
            else
                fuzz_err = 1;
        }

    } else if (currentPt->choice != ETokenType::eNumber) {
        /* this prevents endless cycling, unconditionally */
        if (currentPt->choice != ETokenType::eOneOf && currentPt->choice != ETokenType::eOneOfNum)
            ++currentPt;
        num_found = -1;
    }

    if (! strange_sin_dot) {
        if (currentPt == end_it) {
            xgbparse_error("unexpected end of interval tokens",
                           tokens,
                           currentPt);
            keep_rawPt = true;
            ++numErrors;
        } else {
            if (currentPt->choice == ETokenType::eNumber) {
                numPt = fta_atoi(currentPt->data.c_str()) - 1;
                ++currentPt;
                num_found = 1;
            }
        }
    }

    if (fuzz_err) {
        xgbparse_error("Incorrect uncertainty", tokens, currentPt);
        keep_rawPt = true;
        ++numErrors;
    }

    if (num_found != 1) {
        keep_rawPt = true;
        /****************
         *
         *  10..one-of(13,15) type syntax here
         *
         ***************/
        if (currentPt->choice == ETokenType::eOneOf || currentPt->choice == ETokenType::eOneOfNum) {
            bool one_of_ok     = true;
            bool at_end_one_of = false;

            ++currentPt;
            if (currentPt->choice != ETokenType::eLeft) {
                one_of_ok = false;
            } else {
                ++currentPt;
            }

            if (one_of_ok && currentPt->choice == ETokenType::eNumber) {
                numPt = fta_atoi(currentPt->data.c_str()) - 1;
                ++currentPt;
            } else {
                one_of_ok = false;
            }

            while (one_of_ok && ! at_end_one_of && currentPt != end_it) {
                switch (currentPt->choice) {
                default:
                    one_of_ok = false;
                    break;
                case ETokenType::eComma:
                case ETokenType::eNumber:
                    ++currentPt;
                    break;
                case ETokenType::eRight:
                    ++currentPt;
                    at_end_one_of = true;
                    break;
                }
            }

            if (! one_of_ok && ! at_end_one_of) {
                while (! at_end_one_of && currentPt != end_it) {
                    if (currentPt->choice == ETokenType::eRight)
                        at_end_one_of = true;
                    ++currentPt;
                }
            }

            if (! one_of_ok) {

                xgbparse_error("bad one-of() syntax as number",
                               tokens,
                               currentPt);
                ++numErrors;
            }
        } else {
            xgbparse_error("Number not found when expected",
                           tokens,
                           currentPt);
            ++numErrors;
        }
    }
}


/*--------------- xgbint_ver ()--------------------*/
/* sometimes returns points */
static CRef<CSeq_loc> xgbint_ver(
    bool&             keep_rawPt,
    TTokenIt&         currentPt,
    const TTokens&    tokens,
    int&              numErrors,
    const TSeqIdList& seq_ids,
    bool              accver)
{
    auto ret = Ref(new CSeq_loc());

    CRef<CSeq_id>   new_id;
    CRef<CInt_fuzz> new_fuzz;
    auto            end_it = end(tokens);

    if (currentPt->choice == ETokenType::eAccession) {
        if (accver && currentPt->data.find('.') >= currentPt->data.size() - 1) {
            xgbparse_error("Missing accession's version",
                           tokens,
                           currentPt);
        }

        new_id = Ref(new CSeq_id(currentPt->data));

        ++currentPt;
        if (currentPt == end_it) {
            xgbparse_error("Nothing after accession",
                           tokens,
                           currentPt);
            new_id.Reset();
            keep_rawPt = true;
            ++numErrors;
            return {};
        }

    } else if (! seq_ids.empty()) {
        new_id.Reset(new CSeq_id());
        new_id->Assign(*(*seq_ids.begin()));
    }

    if (currentPt->choice == ETokenType::eLt) {
        new_fuzz.Reset(new CInt_fuzz);
        new_fuzz->SetLim(CInt_fuzz::eLim_lt);

        ++currentPt;
        if (currentPt == end_it) {
            xgbparse_error("Nothing after \'<\'",
                           tokens,
                           currentPt);
            keep_rawPt = true;
            ++numErrors;
            return {};
        }
    }

    if (! numErrors) {
        switch (currentPt->choice) {
        case ETokenType::eAccession:
            if (new_id.NotEmpty()) {
                xgbparse_error("duplicate accessions",
                               tokens,
                               currentPt);
                keep_rawPt = true;
                ++numErrors;
                return {};
            }
            break;
        case ETokenType::eCaret:
            xgbparse_error("caret (^) before number",
                           tokens,
                           currentPt);
            keep_rawPt = true;
            ++numErrors;
            return {};
        case ETokenType::eLt:
            if (new_id.NotEmpty()) {
                xgbparse_error("duplicate \'<\'",
                               tokens,
                               currentPt);
                keep_rawPt = true;
                ++numErrors;
                return {};
            }
            break;
        case ETokenType::eGt:
        case ETokenType::eNumber:
        case ETokenType::eLeft:

        case ETokenType::eOneOfNum:
            if (new_fuzz.NotEmpty())
                ret->SetInt().SetFuzz_from(*new_fuzz);
            if (new_id.NotEmpty())
                ret->SetInt().SetId(*new_id);

            xgbload_number(ret->SetInt().SetFrom(), ret->SetInt().SetFuzz_from(), keep_rawPt, currentPt, tokens, numErrors, TAKE_FIRST);

            if (ret->GetInt().GetFuzz_from().Which() == CInt_fuzz::e_not_set)
                ret->SetInt().ResetFuzz_from();

            xgbcheck_range(ret->GetInt().GetFrom(), *new_id, keep_rawPt, numErrors, tokens, currentPt);

            if (numErrors) {
                return {};
            }

            if (currentPt != end_it) {
                bool in_caret = false;
                switch (currentPt->choice) {
                default:
                case ETokenType::eJoin:
                case ETokenType::eCompl:
                case ETokenType::eSingleDot:
                case ETokenType::eOrder:
                case ETokenType::eGroup:
                case ETokenType::eAccession:
                    xgbparse_error("problem with 2nd number",
                                   tokens,
                                   currentPt);
                    keep_rawPt = true;
                    ++numErrors;
                    return {};
                case ETokenType::eComma:
                case ETokenType::eRight: /* valid thing to leave on*/
                    /*--------------but have a point, not an interval----*/
                    sConvertIntToPoint(*ret);
                    break;

                case ETokenType::eGt:
                case ETokenType::eLt:
                    xgbparse_error("Missing \'..\'",
                                   tokens,
                                   currentPt);
                    keep_rawPt = true;
                    ++numErrors;
                    return {};
                case ETokenType::eCaret:
                    if (ret->GetInt().IsSetFuzz_from()) {
                        xgbparse_error("\'<\' then \'^\'",
                                       tokens,
                                       currentPt);
                        keep_rawPt = true;
                        ++numErrors;
                        return {};
                    }

                    ret->SetInt().SetFuzz_from().SetLim(CInt_fuzz::eLim_tl);
                    ret->SetInt().SetFuzz_to().SetLim(CInt_fuzz::eLim_tl);
                    in_caret = true;
                    /*---no break on purpose ---*/
                    NCBI_FALLTHROUGH;
                case ETokenType::eDotDot:
                    ++currentPt;
                    if (currentPt == end_it) {
                        xgbparse_error("unexpected end of usable tokens",
                                       tokens,
                                       currentPt);
                        keep_rawPt = true;
                        ++numErrors;
                        return {};
                    }
                    /*--no break on purpose here ---*/
                    NCBI_FALLTHROUGH;
                case ETokenType::eNumber:
                case ETokenType::eLeft:

                case ETokenType::eOneOfNum: /* unlikely, but ok */
                    if (currentPt->choice == ETokenType::eRight) {
                        if (ret->GetInt().IsSetFuzz_from()) {
                            xgbparse_error("\'^\' then \'>\'",
                                           tokens,
                                           currentPt);
                            keep_rawPt = true;
                            ++numErrors;
                            return {};
                        }
                    }

                    xgbload_number(ret->SetInt().SetTo(), ret->SetInt().SetFuzz_to(), keep_rawPt, currentPt, tokens, numErrors, TAKE_SECOND);

                    if (ret->GetInt().GetFuzz_to().Which() == CInt_fuzz::e_not_set)
                        ret->SetInt().ResetFuzz_to();

                    xgbcheck_range(ret->GetInt().GetTo(), *new_id, keep_rawPt, numErrors, tokens, currentPt);

                    /*----------
                     *  The caret location implies a place (point) between two location.
                     *  This is not exactly captured by the ASN.1, but pretty close
                     *-------*/
                    if (in_caret) {
                        TSeqPos to = ret->GetInt().GetTo();

                        sConvertIntToPoint(*ret);
                        CSeq_point& point = ret->SetPnt();
                        if (point.GetPoint() + 1 == to) {
                            point.SetPoint(to); /* was essentailly correct */
                        } else {
                            point.SetFuzz().SetRange().SetMax(to);
                            point.SetFuzz().SetRange().SetMin(point.GetPoint());
                        }
                    }

                    if (ret->IsInt() &&
                        ret->GetInt().GetFrom() == ret->GetInt().GetTo() &&
                        ! ret->GetInt().IsSetFuzz_from() &&
                        ! ret->GetInt().IsSetFuzz_to()) {
                        /*-------if interval really a point, make is so ----*/
                        sConvertIntToPoint(*ret);
                    }
                } /* end switch */
            } else {
                sConvertIntToPoint(*ret);
            }
            break;
        default:
            xgbparse_error("No number when expected",
                           tokens,
                           currentPt);
            keep_rawPt = true;
            ++numErrors;
            return {};
        }
    }

    return ret;
}


class CGBLocException : exception
{
};

static CRef<CSeq_loc> xgbloc_ver(bool& keep_rawPt, int& parenPt, TTokenIt& currentPt, const TTokens& tokens, int& numErrors, const TSeqIdList& seq_ids, bool accver)
{
    CRef<CSeq_loc> retval;

    bool add_nulls      = false;
    auto current_token  = currentPt;
    bool did_complement = false;
    bool in_sites;
    auto end_it = end(tokens);

    try {
        do {
            in_sites = false;
            switch (current_token->choice) {
            case ETokenType::eCompl:
                ++currentPt;
                if (currentPt == end_it) {
                    xgbparse_error("unexpected end of usable tokens",
                                   tokens,
                                   currentPt);
                    throw CGBLocException();
                }
                if (currentPt->choice != ETokenType::eLeft) {
                    xgbparse_error("Missing \'(\'", /* paran match  ) */
                                   tokens,
                                   currentPt);
                    throw CGBLocException();
                }

                ++parenPt;
                ++currentPt;
                if (currentPt == end_it) {
                    xgbparse_error("illegal null contents",
                                   tokens,
                                   currentPt);
                    throw CGBLocException();
                }

                if (currentPt->choice == ETokenType::eRight) { /* paran match ( */
                    xgbparse_error("Premature \')\'",
                                   tokens,
                                   currentPt);
                    throw CGBLocException();
                }
                retval = xgbloc_ver(keep_rawPt, parenPt, currentPt, tokens, numErrors, seq_ids, accver);

                if (retval.NotEmpty())
                    retval = sequence::SeqLocRevCmpl(*retval, nullptr);

                did_complement = true;
                if (currentPt != end_it) {
                    if (currentPt->choice != ETokenType::eRight) {
                        xgbparse_error("Missing \')\'",
                                       tokens,
                                       currentPt);
                        throw CGBLocException();
                    }
                    --parenPt;
                    ++currentPt;
                } else {
                    xgbparse_error("Missing \')\'",
                                   tokens,
                                   currentPt);
                    throw CGBLocException();
                }
                break;
                /* REAL LOCS */
            case ETokenType::eJoin:
                retval = Ref(new CSeq_loc());
                retval->SetMix();
                break;
            case ETokenType::eOrder:
                retval = Ref(new CSeq_loc);
                retval->SetMix();
                add_nulls = true;
                break;
            case ETokenType::eGroup:
                retval = Ref(new CSeq_loc);
                retval->SetMix();
                keep_rawPt = true;
                break;
            case ETokenType::eOneOf:
                retval = Ref(new CSeq_loc);
                retval->SetEquiv();
                break;

            /* ERROR */
            case ETokenType::eString:
                xgbparse_error("string in loc",
                               tokens,
                               current_token);
                throw CGBLocException();
            /*--- no break on purpose---*/
            default:
            case ETokenType::eUnknown:
            case ETokenType::eRight:
            case ETokenType::eDotDot:
            case ETokenType::eComma:
            case ETokenType::eSingleDot:
                xgbparse_error("illegal initial loc token",
                               tokens,
                               currentPt);
                throw CGBLocException();

            /* Interval, occurs on recursion */
            case ETokenType::eGap:
                xgbgap(currentPt, end_it, retval, false);
                break;
            case ETokenType::eUnkGap:
                xgbgap(currentPt, end_it, retval, true);
                break;

            case ETokenType::eAccession:
            case ETokenType::eCaret:
            case ETokenType::eGt:
            case ETokenType::eLt:
            case ETokenType::eNumber:
            case ETokenType::eLeft:
            case ETokenType::eOneOfNum:
                retval = xgbint_ver(keep_rawPt, currentPt, tokens, numErrors, seq_ids, accver);
                break;

            case ETokenType::eReplace:
                /*-------illegal at this level --*/
                xgbparse_error("illegal replace",
                               tokens,
                               currentPt);
                throw CGBLocException();
            case ETokenType::eSites:
                in_sites = true;
                ++currentPt;
                break;
            }
        } while (in_sites && currentPt != end_it);

        if (! numErrors && ! did_complement && retval &&
            ! retval->IsNull() && ! retval->IsInt() && ! retval->IsPnt()) {
            /*--------
             * ONLY THE CHOICE has been set. the "join", etc. only has been noted
             *----*/
            ++currentPt;
            if (currentPt == end_it) {
                xgbparse_error("unexpected end of interval tokens",
                               tokens,
                               currentPt);
                throw CGBLocException();
            }

            if (currentPt->choice != ETokenType::eLeft) {
                xgbparse_error("Missing \'(\'",
                               tokens,
                               currentPt); /* paran match  ) */
                throw CGBLocException();
            }

            ++parenPt;
            ++currentPt;
            if (currentPt == end_it) {
                xgbparse_error("illegal null contents",
                               tokens,
                               currentPt);
                throw CGBLocException();
            }

            if (currentPt->choice == ETokenType::eRight) { /* paran match ( */
                xgbparse_error("Premature \')\'",
                               tokens,
                               currentPt);
                throw CGBLocException();
            }

            while (! numErrors && currentPt != end_it) {
                if (currentPt->choice == ETokenType::eRight) {
                    while (currentPt != end_it &&
                           currentPt->choice == ETokenType::eRight) {
                        --parenPt;
                    }
                    break;
                }

                if (currentPt == end_it)
                    break;

                CRef<CSeq_loc> next_loc = xgbloc_ver(keep_rawPt, parenPt, currentPt, tokens, numErrors, seq_ids, accver);
                if (next_loc.NotEmpty()) {
                    if (retval->IsMix())
                        retval->SetMix().AddSeqLoc(*next_loc);
                    else // equiv
                        retval->SetEquiv().Add(*next_loc);
                }

                if (currentPt == end_it || currentPt->choice == ETokenType::eRight)
                    break;

                if (currentPt->choice == ETokenType::eComma) {
                    ++currentPt;
                    if (add_nulls) {
                        CRef<CSeq_loc> null_loc(new CSeq_loc);
                        null_loc->SetNull();

                        if (retval->IsMix())
                            retval->SetMix().AddSeqLoc(*null_loc);
                        else // equiv
                            retval->SetEquiv().Add(*null_loc);
                    }
                } else {
                    xgbparse_error("Illegal token after interval",
                                   tokens,
                                   currentPt);
                    throw CGBLocException();
                }
            }

            if (currentPt == end_it) {
                xgbparse_error("unexpected end of usable tokens",
                               tokens,
                               currentPt);
                throw CGBLocException();
            }

            if (currentPt->choice != ETokenType::eRight) {
                xgbparse_error("Missing \')\'" /* paran match  ) */,
                               tokens,
                               currentPt);
                throw CGBLocException();
            }
            --parenPt;
            ++currentPt;
        }
    } catch (CGBLocException&) {
        keep_rawPt = true;
        ++numErrors;
        if (retval) {
            retval->Reset();
            retval->SetWhole().Assign(*(*seq_ids.begin()));
        }
    }

    return retval;
}


static CRef<CSeq_loc> xgbreplace_ver(bool& keep_rawPt, int& parenPt, TTokenIt& currentPt, const TTokens& tokens, int& numErrors, const TSeqIdList& seq_ids, bool accver)
{
    CRef<CSeq_loc> ret;

    keep_rawPt = true;
    ++currentPt;

    if (currentPt->choice == ETokenType::eLeft) {
        ++currentPt;
        ret = xgbloc_ver(keep_rawPt, parenPt, currentPt, tokens, numErrors, seq_ids, accver);

        if (currentPt == end(tokens)) {
            xgbparse_error("unexpected end of interval tokens",
                           tokens,
                           currentPt);
            keep_rawPt = true;
            ++numErrors;
        } else if (currentPt->choice != ETokenType::eComma) {
            xgbparse_error("Missing comma after first location in replace",
                           tokens,
                           currentPt);
            ++numErrors;
        }
    } else {
        xgbparse_error("Missing \'(\'" /* paran match  ) */
                       ,
                       tokens,
                       currentPt);
        ++numErrors;
    }

    return ret;
}


/*---------- xgbparseint_ver()-----*/

CRef<CSeq_loc> xgbparseint_ver(string_view raw_intervals, bool& keep_rawPt, int& numErrors, const TSeqIdList& seq_ids, bool accver)
{
    keep_rawPt = false;

    TTokens tokens;
    numErrors = xgbparselex_ver(raw_intervals, tokens, accver);

    if (tokens.empty()) {
        numErrors = 1;
        return {};
    }

    if (numErrors) {
        keep_rawPt = true;
        return {};
    }

    CRef<CSeq_loc> ret;
    xfind_one_of_num(tokens);
    auto head_token    = tokens.begin();
    auto current_token = head_token;
    auto end_it        = tokens.end();

    int  paren_count = 0;
    bool in_sites;
    do {
        in_sites = false;
        if (current_token != end_it) {
            switch (current_token->choice) {
            case ETokenType::eJoin:
            case ETokenType::eOrder:
            case ETokenType::eGroup:
            case ETokenType::eOneOf:
            case ETokenType::eCompl:
                ret = xgbloc_ver(keep_rawPt, paren_count, current_token, tokens, numErrors, seq_ids, accver);
                /* need to check that out of tokens here */
                xgbparse_better_be_done(numErrors, current_token, tokens, keep_rawPt, paren_count);
                break;

            case ETokenType::eString:
                xgbparse_error("string in loc", tokens, current_token);
                keep_rawPt = true;
                ++numErrors;
                /*  no break on purpose */
                NCBI_FALLTHROUGH;
            case ETokenType::eUnknown:
            default:
            case ETokenType::eRight:
            case ETokenType::eDotDot:
            case ETokenType::eComma:
            case ETokenType::eSingleDot:
                xgbparse_error("illegal initial token", tokens, current_token);
                keep_rawPt = true;
                ++numErrors;
                ++current_token;
                break;

            case ETokenType::eAccession:
                /*--- no warn, but strange ---*/
                /*-- no break on purpose ---*/

            case ETokenType::eCaret:
            case ETokenType::eGt:
            case ETokenType::eLt:
            case ETokenType::eNumber:
            case ETokenType::eLeft:
            case ETokenType::eOneOfNum:
                ret = xgbint_ver(keep_rawPt, current_token, tokens, numErrors, seq_ids, accver);
                /* need to check that out of tokens here */
                xgbparse_better_be_done(numErrors, current_token, tokens, keep_rawPt, paren_count);
                break;

            case ETokenType::eReplace:
                ret        = xgbreplace_ver(keep_rawPt, paren_count, current_token, tokens, numErrors, seq_ids, accver);
                keep_rawPt = true;
                /*---all errors handled within this function ---*/
                break;
            case ETokenType::eSites:
                in_sites = true;
                ++current_token;
                break;
            }
        }
    } while (in_sites && current_token != end_it);

    if (numErrors) {
        return CRef<CSeq_loc>();
    }

    return ret;
}

END_NCBI_SCOPE
