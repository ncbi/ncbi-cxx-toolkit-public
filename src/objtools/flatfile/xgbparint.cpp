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
//#include "valnode.h"
#include "xgbparint.h"

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "xgbparint.cpp"

#define TAKE_FIRST 1
#define TAKE_SECOND 2

#define GBPARSE_INT_UNKNOWN 0
#define GBPARSE_INT_JOIN 1
#define GBPARSE_INT_COMPL 2
#define GBPARSE_INT_LEFT 3
#define GBPARSE_INT_RIGHT 4
#define GBPARSE_INT_CARET 5
#define GBPARSE_INT_DOT_DOT 6
#define GBPARSE_INT_ACCESSION 7
#define GBPARSE_INT_GT 8
#define GBPARSE_INT_LT 9
#define GBPARSE_INT_COMMA 10
#define GBPARSE_INT_NUMBER 11
#define GBPARSE_INT_ORDER 12
#define GBPARSE_INT_SINGLE_DOT 13
#define GBPARSE_INT_GROUP 14
#define GBPARSE_INT_ONE_OF 15
#define GBPARSE_INT_REPLACE 16
#define GBPARSE_INT_SITES 17
#define GBPARSE_INT_STRING 18
#define GBPARSE_INT_ONE_OF_NUM 19
#define GBPARSE_INT_GAP 20
#define GBPARSE_INT_UNK_GAP 21

#define ERR_NCBIGBPARSE_LEX 1
#define ERR_NCBIGBPARSE_INT 2

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

const char* seqlitdbtag = "SeqLit";
const char* unkseqlitdbtag = "UnkSeqLit";


struct STokenInfo 
{
/*
    enum EChoice {
        eUnknown,   // 0 
        eJoin,      // 1
        eCompl,     // 2
        eLeft,      // 3
        eRight,     // 4
        eCaret,     // 5
        eDotDot,    // 6
        eAccession, // 7
        eGt,        // 8
        eLt,        // 9
        eComma,     // 10
        eNumber,    // 11
        eOrder,     // 12
        eSingleDot, // 13
        eGroup,     // 14
        eOneOf,     // 15
        eReplace,   // 16
        eSites,     // 17
        eString,    // 18
        eOneOfNum,  // 19
        eGap,       // 20
        eUnkGap     // 21
    };

    EChoice choice;
*/
    unsigned char choice;
    //int choice;
    string data;
};

using TTokens = list<STokenInfo>;
using TTokenIt = TTokens::iterator;



/*--------- do_xgbparse_error () ---------------*/

#define ERR_FEATURE_LocationParsing_validatr 1,5

static void do_xgbparse_error (const char* msg, const char* details)
{
    string errmsg = string(msg) + " at " + string(details);
    Nlm_ErrSetContext("validatr", __FILE__, __LINE__);
    Nlm_ErrPostEx(SEV_ERROR, ERR_FEATURE_LocationParsing_validatr, errmsg.c_str());

}

static X_gbparse_errfunc Err_func = do_xgbparse_error;
static X_gbparse_rangefunc Range_func = NULL;
static void* xgbparse_range_data = NULL;

/*----------- xinstall_gbparse_error_handler ()-------------*/

void xinstall_gbparse_error_handler(X_gbparse_errfunc new_func)
{
    Err_func = new_func;
}

/*----------- xinstall_gbparse_range_func ()-------------*/

void xinstall_gbparse_range_func(void* data, X_gbparse_rangefunc new_func)
{
    Range_func = new_func;
    xgbparse_range_data = data;
}

/*------ xgbparse_point ()----*/
static string xgbparse_point(TTokenIt head, TTokenIt current)
{
    string temp;
    auto end_it = next(current);
    for (auto it = head; it != end_it; ++it){
        switch (it->choice){
        case GBPARSE_INT_JOIN:
            temp += "join";
            break;
        case GBPARSE_INT_COMPL:
            temp += "complement";
            break;
        case GBPARSE_INT_LEFT:
            temp += "(";
            break;
        case GBPARSE_INT_RIGHT:
            temp += ")";
            break;
        case GBPARSE_INT_CARET:
            temp += "^";
            break;
        case GBPARSE_INT_DOT_DOT:
            temp += "..";
            break;
        case GBPARSE_INT_ACCESSION:
        case GBPARSE_INT_NUMBER:
        case GBPARSE_INT_STRING:
            temp += it->data;
            break;
        case GBPARSE_INT_GT:
            temp += ">";
            break;
        case GBPARSE_INT_LT:
            temp += "<";
            break;
        case GBPARSE_INT_COMMA:
            temp += ",";
            break;
        case GBPARSE_INT_ORDER:
            temp += "order";
            break;
        case GBPARSE_INT_SINGLE_DOT:
            temp += ".";
            break;
        case GBPARSE_INT_GROUP:
            temp  += "group";
            break;
        case GBPARSE_INT_ONE_OF:
        case GBPARSE_INT_ONE_OF_NUM:
            temp += "one-of";
            break;
        case GBPARSE_INT_REPLACE:
            temp += "replace";
            break;
        case GBPARSE_INT_UNKNOWN:
        default:
            break;
        }
        temp += " ";
        if (it == current)
            break;
    }

    return temp;
}



static void xgbparse_error(const char* front, TTokenIt head, TTokenIt current)
{
    string details = xgbparse_point(head, current);
    Err_func(front, details.c_str());
}


/*------------------ xgbcheck_range()-------------*/
static void xgbcheck_range(TSeqPos num, const CSeq_id& id, bool& keep_rawPt, int& num_errsPt, TTokenIt head, TTokenIt current)
{
     if (!Range_func) {
        return;
     }

     const auto len = (*Range_func)(xgbparse_range_data, id);
     if (len != -1 && num >= len) {
         xgbparse_error("range error", head, current);
         keep_rawPt = true;
         ++num_errsPt;
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

To deal with this, the GBPARSE_INT_ONE_OF token type will be changed
by the following function to GBPARSE_INT_ONE_OF_NUM, in the three cases.

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
    if (current_it->choice == GBPARSE_INT_ONE_OF) {
        for (auto scanner_it = next(current_it); 
                scanner_it != end(tokens); 
                ++scanner_it) {
            if (scanner_it->choice == GBPARSE_INT_RIGHT) {
                ++scanner_it;
                if (scanner_it != end(tokens) &&
                    scanner_it->choice == GBPARSE_INT_DOT_DOT) {
                    current_it->choice = GBPARSE_INT_ONE_OF_NUM;
                }
                break;
            }
        }
    }

    for (auto current_it = begin(tokens); 
            current_it != end(tokens); 
            ++current_it) {
        if (current_it->choice == GBPARSE_INT_COMMA ||
            current_it->choice == GBPARSE_INT_LEFT) {
            auto scanner_it = next(current_it);
            if (scanner_it != end(tokens)) {
                if (scanner_it->choice == GBPARSE_INT_ONE_OF) {
                    ++scanner_it;
                    while (scanner_it != end(tokens)) {
                        if (scanner_it->choice == GBPARSE_INT_RIGHT) {
                            ++scanner_it;
                            if (scanner_it != end(tokens) &&
                                scanner_it->choice == GBPARSE_INT_DOT_DOT) {
                                current_it->choice = GBPARSE_INT_ONE_OF_NUM;
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



static void xlex_error_func(const char* msg, 
        const string& line, 
        const int current_col)
{
   string temp_string = line.substr(0, current_col+1) + " ";
   Err_func(msg, temp_string.c_str());
}


static int advance_to(const char c, int current_pos, const string& line)
{   
    int pos = current_pos;
    while (pos < line.size()) {
        if(line[pos] == c)  {
           return pos-1; 
        }
        ++pos;
    }
    return pos-1;
}


/**********************************************************/
static size_t xgbparse_accprefix(const char* acc)
{
    const char* p;

    if (acc == NULL || *acc == '\0')
        return(0);

    for (p = acc; isalpha(*p) != 0;)
        p++;
    size_t ret = p - acc;
    if (*p == '_')
    {
        if (ret == 2)
        {
            for (p++; isalpha(*p) != 0;)
                p++;
            ret = p - acc;
            if (ret != 3 && ret != 7)
                ret = 1;
        }
        else
            ret = 1;
    }
    else if (p[0] != '\0' && p[0] >= '0' && p[0] <= '9' &&
             p[1] != '\0' && p[1] >= '0' && p[1] <= '9' && p[2] == 'S')
             ret = 7;
    else if (ret != 1 && ret != 2 && ret != 4 && ret != 6)
        ret = 1;
    return(ret);
}


static size_t sParseAccessionPrefix(const CTempString& accession)
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
    }
    else if (accession.size() >= 3 && 
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


static int sGetAccession(string& accession, int& current_col, const string& line, bool accver)
{
    const auto length = line.size();
    CTempString tempString(line.c_str() + current_col, length-current_col);
    auto prefixLength = sParseAccessionPrefix(tempString);
    size_t accessionLength = prefixLength;

    tempString = tempString.substr(prefixLength);
    auto notDigitPos = tempString.find_first_not_of("0123456789"); 
    if (notDigitPos != NPOS) {
        accessionLength += notDigitPos;
        if (accver && tempString[notDigitPos] == '.') {
            ++accessionLength;
            if (tempString.size() > notDigitPos) {
                tempString = tempString.substr(notDigitPos+1);
                notDigitPos = tempString.find_first_not_of("0123456789");
                if (notDigitPos != NPOS) {
                    accessionLength += notDigitPos;
                }
            }
        }
    }
    else {
        accessionLength = length - current_col;
    }

    int retval = 0;
    if (notDigitPos == NPOS || tempString[notDigitPos] != ':') {
        xlex_error_func("ACCESSION missing \":\"", line, current_col);
        ++retval; 
        --current_col;
    } 

    accession = string(line.c_str() + current_col, accessionLength);
    current_col += accessionLength;
    return retval;

}


/*------------- xgbparselex_ver() -----------------------*/

static int xgbparselex_ver(const char* linein, TTokens& tokens, bool accver)
{   
    tokens.clear();
    int retval = 0;
    if (*linein)
    {
        string line{ linein };
        NStr::TruncateSpacesInPlace(line);
        auto length = line.size();
        int current_col = 0;

        while (current_col < length) {

            if (isspace(line[current_col]) || line[current_col] == '~') {
                ++current_col;
                continue;
            }

            STokenInfo current_token;
            if (isdigit(line[current_col])) {
                current_token.choice = GBPARSE_INT_NUMBER;
                CTempString tempString(line.c_str()+current_col, length-current_col);
                auto not_digit_pos = tempString.find_first_not_of("0123456789");
                int num_digits = (not_digit_pos == NPOS) ? 
                    length - current_col :
                    not_digit_pos;
                current_token.data = string(line.c_str() + current_col, num_digits);
                tokens.push_back(current_token);
                current_col += num_digits;
                continue;
            }

            bool skip_new_token = false;
            switch (line[current_col]){
            case '\"':
                current_token.choice = GBPARSE_INT_STRING;
                if (auto closing_quote_pos = line.find('\"', current_col+1); 
                        closing_quote_pos == string::npos) {
                    xlex_error_func("unterminated string", line, current_col);
                    retval++;
                }
                else {
                    size_t len = closing_quote_pos - current_col + 1;
                    current_token.data = string(line.c_str(), + current_col);
                    current_col += len;
                }
                break;
                /*------
                *  JOIN
                *------*/
            case 'j':
                current_token.choice = GBPARSE_INT_JOIN;
                if (!NStr::StartsWith(line.c_str() + current_col, "join")){
                    xlex_error_func("\"join\" misspelled", line, current_col);
                    retval++;
                    current_col = advance_to('(', current_col, line);
                }
                else{
                    current_col += 3;
                }
                break;

                /*------
                *  ORDER and ONE-OF
                *------*/
            case 'o':
                if (!NStr::StartsWith(line.c_str() + current_col, "order")){
                    if (!NStr::StartsWith(line.c_str() + current_col, "one-of")){
                        xlex_error_func("\"order\" or \"one-of\" misspelled",
                                line, current_col);
                        retval++;
                        current_col = advance_to('(', current_col, line);
                    }
                    else{
                        current_token.choice = GBPARSE_INT_ONE_OF;
                        current_col += 5;
                    }
                }
                else{
                    current_token.choice = GBPARSE_INT_ORDER;
                    current_col += 4;
                }
                break;

                /*------
                *  REPLACE
                *------*/
            case 'r':
                current_token.choice = GBPARSE_INT_REPLACE;
                if (!NStr::StartsWith(line.c_str() + current_col, "replace")){
                    xlex_error_func("\"replace\" misspelled", line, current_col);
                    retval++;
                    current_col = advance_to('(', current_col, line);
                }
                else{
                    current_col += 6;
                }
                break;

                /*------
                *  GAP or GROUP or GI
                *------*/
            case 'g':
                if (NStr::StartsWith(line.c_str() + current_col, "gap") &&
                    (current_col < length-3) &&
                    (line[current_col+3] == '(' ||
                     line[current_col+3] == ' ' ||
                     line[current_col+3] == '\t' ||
                     line[current_col+3] == '\0'))
                {
                    current_token.choice = GBPARSE_INT_GAP;
                    current_token.data = "gap";
                    if (NStr::StartsWith(line.c_str() + current_col+3, "(unk", NStr::eNocase))     
                    {
                        current_token.choice = GBPARSE_INT_UNK_GAP;
                        tokens.push_back(current_token);
                        
                        current_token.choice = GBPARSE_INT_LEFT;
                        current_col += 4;
                    }
                    current_col += 2;
                    break;
                }
                if (NStr::StartsWith(line.c_str() + current_col, "gi|")) {
                    current_token.choice = GBPARSE_INT_ACCESSION;
                    current_col += 3;
                    for (; isdigit(line[current_col]); current_col++);
                    break;
                }
                current_token.choice = GBPARSE_INT_GROUP;
                if (!NStr::StartsWith(line.c_str() + current_col, "group")){
                    xlex_error_func("\"group\" misspelled", line, current_col);
                    retval++;
                    current_col = advance_to('(', current_col, line);
                }
                else{
                    current_col += 4;
                }
                break;

                /*------
                *  COMPLEMENT
                *------*/
            case 'c':
                current_token.choice = GBPARSE_INT_COMPL;
                if (!NStr::StartsWith(line.c_str() + current_col, "complement")){
                    xlex_error_func("\"complement\" misspelled", line, current_col);
                    ++retval;
                    current_col = advance_to('(', current_col, line);
                }
                else{
                    current_col += 9;
                }
                break;

                /*-------
                * internal bases ignored
                *---------*/
            case 'b':
                if (!NStr::StartsWith(line.c_str() + current_col, "bases")){
                    current_token.choice = GBPARSE_INT_ACCESSION;
                    retval += sGetAccession(current_token.data, current_col, line, accver);
                }
                else{
                    skip_new_token = true;
                    current_col += 4;
                }
                break;

                /*------
                *  ()^.,<>  (bases (sites
                *------*/
            case '(':
                if (NStr::StartsWith(line.c_str() + current_col, "(base")){
                    current_token.choice = GBPARSE_INT_JOIN;
                    current_col += 4;
                    if (current_col < length-1 && line[current_col+1] == 's') {
                        current_col++;
                    }
                    tokens.push_back(current_token);
                    current_token.choice = GBPARSE_INT_LEFT;
                }
                else if (NStr::StartsWith(line.c_str() + current_col, "(sites")){
                    current_col += 5;
                    if (current_col < length-1)
                    {
                        if (line[current_col + 1] == ')'){
                            current_col++;
                            current_token.choice = GBPARSE_INT_SITES;
                        }
                        else{
                            current_token.choice = GBPARSE_INT_SITES;
                            tokens.push_back(current_token);
                            current_token.choice = GBPARSE_INT_JOIN;
                            tokens.push_back(current_token);
                            current_token.choice = GBPARSE_INT_LEFT;
                            if (current_col < length-1){
                                if (line[current_col + 1] == ';'){
                                    current_col++;
                                }
                                else if (NStr::StartsWith(line.c_str() + current_col + 1, " ;")){
                                    current_col += 2;
                                }
                            }
                        }
                    }
                }
                else{
                    current_token.choice = GBPARSE_INT_LEFT;
                }
                break;

            case ')':
                current_token.choice = GBPARSE_INT_RIGHT;
                break;

            case '^':
                current_token.choice = GBPARSE_INT_CARET;
                break;

            case '-':
                current_token.choice = GBPARSE_INT_DOT_DOT;
                break;
            case '.':
                if (current_col==length-1 || line[current_col+1] != '.' ){
                    current_token.choice = GBPARSE_INT_SINGLE_DOT;
                }
                else{
                    current_token.choice = GBPARSE_INT_DOT_DOT;
                    current_col++;
                }
                break;

            case '>':
                current_token.choice = GBPARSE_INT_GT;
                break;

            case '<':
                current_token.choice = GBPARSE_INT_LT;
                break;

            case ';':
            case ',':
                current_token.choice = GBPARSE_INT_COMMA;
                break;

            case 't':
                if (!NStr::StartsWith(line.c_str() + current_col, "to")){
                    current_token.choice = GBPARSE_INT_ACCESSION;
                    retval += sGetAccession(current_token.data, current_col, line, accver);
                }
                else{
                    current_token.choice = GBPARSE_INT_DOT_DOT;
                    current_col++;
                }
                break;

            case 's':
                if (!NStr::StartsWith(line.c_str() + current_col, "site")){
                    current_token.choice = GBPARSE_INT_ACCESSION;
                    retval += sGetAccession(current_token.data, current_col, line, accver);
                }
                else{
                    current_token.choice = GBPARSE_INT_SITES;
                    current_col += 3;
                    if (current_col < length-1 && line[current_col+1] == 's'){
                        current_col++;
                    }
                    if (current_col < length-1){
                        if (line[current_col + 1] == ';'){
                            current_col++;
                        }
                        else if (NStr::StartsWith(line.c_str() + current_col + 1, " ;")){
                            current_col += 2;
                        }
                    }
                }
                break;

            default:
                current_token.choice = GBPARSE_INT_ACCESSION;
                retval += sGetAccession(current_token.data, current_col, line, accver);
            }


            /*--move to past last "good" character---*/
            ++current_col;
            if (!skip_new_token) {
                tokens.push_back(current_token);
            }
        }
    }

    return retval;
}




/*----------------- xgbparse_better_be_done()-------------*/
static void xgbparse_better_be_done(int& num_errsPt, TTokenIt current_token, 
        TTokenIt head_token, TTokenIt end_it, 
        bool& keep_rawPt, int paren_count)
{
    if (current_token != end_it)
    {
        while (current_token->choice == GBPARSE_INT_RIGHT)
        {
            paren_count--;
            ++current_token;
            if (current_token == end_it)
            {
                if (paren_count)
                {
                    char par_msg[40];
                    sprintf(par_msg, "mismatched parentheses (%d)", paren_count);
                    xgbparse_error(par_msg,
                                   head_token, current_token);
                    keep_rawPt = true;
                    ++num_errsPt;
                }
                break;
            }
        }
    }

    if (paren_count)
    {
        xgbparse_error("text after last legal right parenthesis",
                       head_token, current_token);
        keep_rawPt = true;
        ++num_errsPt;
    }

    if (current_token != end_it)
    {
        xgbparse_error("text after end",
                       head_token, current_token);
        keep_rawPt = true;
        ++num_errsPt;
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
    if (range == 0)
    {
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
static void xgbgap(TTokenIt& current_it,  TTokenIt end_it, CRef<CSeq_loc>& loc,  bool unknown)
{
    auto it = next(current_it);   

    if (distance(it, end_it) < 2) {
        return;
    }

    if (it->choice != GBPARSE_INT_LEFT) {
        return;
    }
    ++it;
    if (it->choice != GBPARSE_INT_NUMBER &&
        it->choice != GBPARSE_INT_RIGHT) {
        return;
    }

    if (it->choice == GBPARSE_INT_RIGHT) {
        loc->SetNull();
    }
    else {
        auto gapsize_it = it++;
        if (it == end_it || it->choice != GBPARSE_INT_RIGHT) {
            return;
        }
        auto pLoc = XGapToSeqLocEx(atoi(gapsize_it->data.c_str()), unknown);
        if (!pLoc) {
            return;
        }
        loc = pLoc;
    }
    current_it = next(it);
}

/*------------------- xgbpintpnt()-----------*/

static void xgbpintpnt(CSeq_loc& loc)
{
    CRef<CSeq_point> point(new CSeq_point);

    point->SetPoint(loc.GetInt().GetFrom());

    if (loc.GetInt().IsSetId())
        point->SetId(loc.SetInt().SetId());

    if (loc.GetInt().IsSetFuzz_from())
        point->SetFuzz(loc.SetInt().SetFuzz_from());

    loc.SetPnt(*point);
}



static void xgbload_number(TSeqPos& numPt, CInt_fuzz& fuzz, bool& keep_rawPt, 
        TTokenIt& currentPt, 
        TTokenIt head_token, 
        TTokenIt end_it,
        int& num_errPt, int take_which)
{
    int num_found = 0;
    int fuzz_err = 0;
    bool strange_sin_dot = false;

    if (currentPt->choice == GBPARSE_INT_CARET)
    {
        xgbparse_error("duplicate carets", head_token, currentPt);
        keep_rawPt = true;
        ++num_errPt;
        ++currentPt;
        fuzz_err = 1;
    }
    else if (currentPt->choice == GBPARSE_INT_GT ||
             currentPt->choice == GBPARSE_INT_LT)
    {
        if (currentPt->choice == GBPARSE_INT_GT)
            fuzz.SetLim(CInt_fuzz::eLim_gt);
        else
            fuzz.SetLim(CInt_fuzz::eLim_lt);

        ++currentPt;
    }
    else if (currentPt->choice == GBPARSE_INT_LEFT)
    {
        strange_sin_dot = true;
        ++currentPt;
        fuzz.SetRange();

        if (currentPt->choice == GBPARSE_INT_NUMBER)
        {
            fuzz.SetRange().SetMin(atoi(currentPt->data.c_str()) - 1);
            if (take_which == TAKE_FIRST)
            {
                numPt = fuzz.GetRange().GetMin();
            }
            ++currentPt;
            num_found = 1;
        }
        else
            fuzz_err = 1;

        if (currentPt->choice != GBPARSE_INT_SINGLE_DOT)
            fuzz_err = 1;
        else
        {
            ++currentPt;
            if (currentPt->choice == GBPARSE_INT_NUMBER)
            {
                fuzz.SetRange().SetMax(atoi(currentPt->data.c_str()) - 1);
                if (take_which == TAKE_SECOND)
                {
                    numPt = fuzz.GetRange().GetMax();
                }
                ++currentPt;
            }
            else
                fuzz_err = 1;

            if (currentPt->choice == GBPARSE_INT_RIGHT)
                ++currentPt;
            else
                fuzz_err = 1;
        }

    }
    else if (currentPt->choice != GBPARSE_INT_NUMBER)
    {
        /* this prevents endless cycling, unconditionally */
        if (currentPt->choice != GBPARSE_INT_ONE_OF
            && currentPt->choice != GBPARSE_INT_ONE_OF_NUM)
            ++currentPt;
        num_found = -1;
    }

    if (!strange_sin_dot)
    {
        if (currentPt == end_it)
        {
            xgbparse_error("unexpected end of interval tokens",
                           head_token, currentPt);
            keep_rawPt = true;
            ++num_errPt;
        }
        else{
            if (currentPt->choice == GBPARSE_INT_NUMBER)
            {
                numPt = atoi(currentPt->data.c_str()) - 1;
                ++currentPt;
                num_found = 1;
            }
        }
    }

    if (fuzz_err)
    {
        xgbparse_error("Incorrect uncertainty", head_token, currentPt);
        keep_rawPt = true;
        ++num_errPt;
    }

    if (num_found != 1)
    {
        keep_rawPt = true;
        /****************
        *
        *  10..one-of(13,15) type syntax here
        *
        ***************/
        if (currentPt->choice == GBPARSE_INT_ONE_OF
            || currentPt->choice == GBPARSE_INT_ONE_OF_NUM)
        {
            bool one_of_ok = true;
            bool at_end_one_of = false;

            ++currentPt;
            if (currentPt->choice != GBPARSE_INT_LEFT)
            {
                one_of_ok = false;
            }
            else
            {
                ++currentPt;
            }

            if (one_of_ok && currentPt->choice == GBPARSE_INT_NUMBER)
            {
                numPt = atoi(currentPt->data.c_str()) - 1;
                ++currentPt;
            }
            else
            {
                one_of_ok = false;
            }

            while (one_of_ok && !at_end_one_of &&  currentPt != end_it)
            {
                switch (currentPt->choice)
                {
                default:
                    one_of_ok = false;
                    break;
                case GBPARSE_INT_COMMA:
                case GBPARSE_INT_NUMBER:
                    ++currentPt;
                    break;
                case GBPARSE_INT_RIGHT:
                    ++currentPt;
                    at_end_one_of = true;
                    break;
                }
            }

            if (!one_of_ok && !at_end_one_of)
            {
                while (!at_end_one_of && currentPt != end_it)
                {
                    if (currentPt->choice == GBPARSE_INT_RIGHT)
                        at_end_one_of = true;
                    ++currentPt;
                }
            }

            if (!one_of_ok){

                xgbparse_error("bad one-of() syntax as number",
                               head_token, currentPt);
                ++num_errPt;
            }
        }
        else
        {
            xgbparse_error("Number not found when expected",
                           head_token, currentPt);
            ++num_errPt;
        }
    }
}




/*--------------- xgbint_ver ()--------------------*/
/* sometimes returns points */
static CRef<CSeq_loc> xgbint_ver(bool& keep_rawPt,
        TTokenIt& currentPt,
        TTokenIt head_token, 
        TTokenIt end_it,
        int& num_errPt, const TSeqIdList& seq_ids,
        bool accver)
{
    CRef<CSeq_loc> ret(new CSeq_loc);

    bool took_choice = false;
    char* p;

    CRef<CSeq_id> new_id;
    CRef<CInt_fuzz> new_fuzz;

    if (currentPt->choice == GBPARSE_INT_ACCESSION)
    {
        CRef<CTextseq_id> text_id(new CTextseq_id);

        if (accver == false)
        {
            text_id->SetAccession(currentPt->data);
        }
        else
        {
            vector<string> acc_ver;
            NStr::Split(currentPt->data, ".", acc_ver);
            if (acc_ver.size() == 1) {
                text_id->SetAccession(currentPt->data);
                xgbparse_error("Missing accession's version",
                               head_token, currentPt);
            }
            else {
                text_id->SetAccession(acc_ver[0]);
                text_id->SetVersion(atoi(acc_ver[1].c_str()));
            }
        }

        new_id.Reset(new CSeq_id);
        if (!seq_ids.empty())
        {
            const CSeq_id& first_id = *(*seq_ids.begin());
            if (first_id.IsEmbl())
            {
                new_id->SetEmbl(*text_id);
                took_choice = true;
            }
            else if (first_id.IsDdbj())
            {
                new_id->SetDdbj(*text_id);
                took_choice = true;
            }
        }

        if (!took_choice) // Genbank
            new_id->SetGenbank(*text_id);

        ++currentPt;
        if (currentPt == end_it)
        {
            xgbparse_error("Nothing after accession",
                           head_token, currentPt);
            new_id.Reset();
            keep_rawPt = true;
            ++num_errPt;
            goto FATAL;
        }
    }
    else if(!seq_ids.empty())
    {
        new_id.Reset(new ncbi::CSeq_id);
        new_id->Assign(*(*seq_ids.begin()));
    }

    if (currentPt->choice == GBPARSE_INT_LT)
    {
        new_fuzz.Reset(new CInt_fuzz);
        new_fuzz->SetLim(CInt_fuzz::eLim_lt);

        ++currentPt;
        if (currentPt == end_it)
        {
            xgbparse_error("Nothing after \'<\'",
                           head_token, currentPt);
            keep_rawPt = true;
            ++num_errPt;
            goto FATAL;
        }
    }

    if (!num_errPt)
    {
        switch (currentPt->choice)
        {
        case  GBPARSE_INT_ACCESSION:
            if (new_id.NotEmpty())
            {
                xgbparse_error("duplicate accessions",
                               head_token, currentPt);
                keep_rawPt = true;
                ++num_errPt;
                goto FATAL;
            }
            break;
        case  GBPARSE_INT_CARET:
            xgbparse_error("caret (^) before number",
                           head_token, currentPt);
            keep_rawPt = true;
            ++num_errPt;
            goto FATAL;
        case  GBPARSE_INT_LT:
            if (new_id.NotEmpty())
            {
                xgbparse_error("duplicate \'<\'",
                               head_token, currentPt);
                keep_rawPt = true;
                ++num_errPt;
                goto FATAL;
            }
            break;
        case  GBPARSE_INT_GT:
        case  GBPARSE_INT_NUMBER:
        case  GBPARSE_INT_LEFT:

        case GBPARSE_INT_ONE_OF_NUM:
            if (new_fuzz.NotEmpty())
                ret->SetInt().SetFuzz_from(*new_fuzz);
            if (new_id.NotEmpty())
                ret->SetInt().SetId(*new_id);

            xgbload_number(ret->SetInt().SetFrom(), ret->SetInt().SetFuzz_from(),
                           keep_rawPt, currentPt, head_token, end_it,
                           num_errPt, TAKE_FIRST);

            if (ret->GetInt().GetFuzz_from().Which() == CInt_fuzz::e_not_set)
                ret->SetInt().ResetFuzz_from();

            xgbcheck_range(ret->GetInt().GetFrom(), *new_id, keep_rawPt, num_errPt, head_token, currentPt);

            if (!num_errPt)
            {
                if (currentPt != end_it)
                {
                    bool in_caret = false;
                    switch (currentPt->choice)
                    {
                    default:
                    case GBPARSE_INT_JOIN:
                    case GBPARSE_INT_COMPL:
                    case GBPARSE_INT_SINGLE_DOT:
                    case GBPARSE_INT_ORDER:
                    case GBPARSE_INT_GROUP:
                    case GBPARSE_INT_ACCESSION:
                        xgbparse_error("problem with 2nd number",
                                       head_token, currentPt);
                        keep_rawPt = true;
                        ++num_errPt;
                        goto FATAL;
                    case GBPARSE_INT_COMMA: case GBPARSE_INT_RIGHT: /* valid thing to leave on*/
                        /*--------------but have a point, not an interval----*/
                        xgbpintpnt(*ret);
                        break;

                    case GBPARSE_INT_GT: case GBPARSE_INT_LT:
                        xgbparse_error("Missing \'..\'",
                                       head_token, currentPt);;
                        keep_rawPt = true;
                        ++num_errPt;
                        goto FATAL;
                    case GBPARSE_INT_CARET:
                        if (ret->GetInt().IsSetFuzz_from())
                        {
                            xgbparse_error("\'<\' then \'^\'",
                                           head_token, currentPt);
                            keep_rawPt = true;
                            ++num_errPt;
                            goto FATAL;
                        }

                        ret->SetInt().SetFuzz_from().SetLim(CInt_fuzz::eLim_tl);
                        ret->SetInt().SetFuzz_to().SetLim(CInt_fuzz::eLim_tl);
                        in_caret = true;
                        /*---no break on purpose ---*/

                    case GBPARSE_INT_DOT_DOT:
                        ++currentPt;
                        if (currentPt == end_it)
                        {
                            xgbparse_error("unexpected end of usable tokens",
                                           head_token, currentPt);
                            keep_rawPt = true;
                            ++num_errPt;
                            goto FATAL;
                        }
                        /*--no break on purpose here ---*/
                    case GBPARSE_INT_NUMBER:
                    case GBPARSE_INT_LEFT:

                    case GBPARSE_INT_ONE_OF_NUM:  /* unlikely, but ok */

                        if (currentPt->choice == GBPARSE_INT_RIGHT)
                        {
                            if (ret->GetInt().IsSetFuzz_from())
                            {
                                xgbparse_error("\'^\' then \'>\'",
                                               head_token, currentPt);
                                keep_rawPt = true;
                                ++num_errPt;
                                goto FATAL;
                            }
                        }

                        xgbload_number(ret->SetInt().SetTo(), ret->SetInt().SetFuzz_to(),
                                       keep_rawPt, currentPt, head_token, end_it,
                                       num_errPt, TAKE_SECOND);

                        if (ret->GetInt().GetFuzz_to().Which() == CInt_fuzz::e_not_set)
                            ret->SetInt().ResetFuzz_to();

                        xgbcheck_range(ret->GetInt().GetTo(), *new_id, keep_rawPt, num_errPt, head_token, currentPt);

                        /*----------
                        *  The caret location implies a place (point) between two location.
                        *  This is not exactly captured by the ASN.1, but pretty close
                        *-------*/
                        if (in_caret)
                        {
                            TSeqPos to = ret->GetInt().GetTo();

                            xgbpintpnt(*ret);
                            CSeq_point& point = ret->SetPnt();
                            if (point.GetPoint() + 1 == to)
                            {
                                point.SetPoint(to); /* was essentailly correct */
                            }
                            else
                            {
                                point.SetFuzz().SetRange().SetMax(to);
                                point.SetFuzz().SetRange().SetMin(point.GetPoint());
                            }
                        }

                        if (ret->IsInt())
                        {
                            if (ret->GetInt().GetFrom() == ret->GetInt().GetTo() &&
                                !ret->GetInt().IsSetFuzz_from() &&
                                !ret->GetInt().IsSetFuzz_to())
                            {
                                /*-------if interval really a point, make is so ----*/
                                xgbpintpnt(*ret);
                            }
                        }
                    } /* end switch */
                }
                else
                {
                    xgbpintpnt(*ret);
                }
            }
            else
            {
                goto FATAL;
            }
            break;
        default:
            xgbparse_error("No number when expected",
                           head_token, currentPt);
            keep_rawPt = true;
            ++num_errPt;
            goto FATAL;

        }
    }


RETURN:
    return ret;

FATAL:
    ret.Reset();
    goto RETURN;
}



static CRef<CSeq_loc> xgbloc_ver(bool& keep_rawPt, int& parenPt,
                                                      bool& sitesPt, TTokenIt& currentPt,
                                                      TTokenIt head_token, TTokenIt end_it, 
                                                      int& num_errPt,
                                                      const TSeqIdList& seq_ids, bool accver)
{
    CRef<CSeq_loc> retval;

    bool add_nulls = false;
    auto current_token = currentPt;
    bool did_complement = false;
    bool go_again;

    do
    {
        go_again = false;
        switch (current_token->choice)
        {
        case  GBPARSE_INT_COMPL:
            ++currentPt;
            if (currentPt == end_it){
                xgbparse_error("unexpected end of usable tokens",
                               head_token, currentPt);
                keep_rawPt = true;
                ++num_errPt;
                goto FATAL;
            }
            if (currentPt->choice != GBPARSE_INT_LEFT){
                xgbparse_error("Missing \'(\'", /* paran match  ) */
                               head_token, currentPt);
                keep_rawPt = true;
                ++num_errPt;
                goto FATAL;
            }
            else{
                ++parenPt; 
                ++currentPt;
                if (currentPt == end_it){
                    xgbparse_error("illegal null contents",
                                   head_token, currentPt);
                    keep_rawPt = true;
                    ++num_errPt;
                    goto FATAL;
                }
                else{
                    if (currentPt->choice == GBPARSE_INT_RIGHT){ /* paran match ( */
                        xgbparse_error("Premature \')\'",
                                       head_token, currentPt);
                        keep_rawPt = true;
                        ++num_errPt;
                        goto FATAL;
                    }
                    else{
                        retval = xgbloc_ver(keep_rawPt, parenPt, sitesPt, currentPt,
                                            head_token, end_it, num_errPt, seq_ids, accver);

                        if (retval.NotEmpty())
                            retval = sequence::SeqLocRevCmpl(*retval, nullptr);

                        did_complement = true;
                        if (currentPt != end_it){
                            if (currentPt->choice != GBPARSE_INT_RIGHT){
                                xgbparse_error("Missing \')\'",
                                               head_token, currentPt);
                                keep_rawPt = true;
                                ++num_errPt;
                                goto FATAL;
                            }
                            else{
                                --parenPt;
                                ++currentPt;
                            }
                        }
                        else{
                            xgbparse_error("Missing \')\'",
                                           head_token, currentPt);
                            keep_rawPt = true;
                            ++num_errPt;
                            goto FATAL;
                        }
                    }
                }
            }
            break;
            /* REAL LOCS */
        case GBPARSE_INT_JOIN:
            retval.Reset(new CSeq_loc);
            retval->SetMix();
            break;
        case  GBPARSE_INT_ORDER:
            retval.Reset(new CSeq_loc);
            retval->SetMix();
            add_nulls = true;
            break;
        case  GBPARSE_INT_GROUP:
            retval.Reset(new CSeq_loc);
            retval->SetMix();
            keep_rawPt = true;
            break;
        case  GBPARSE_INT_ONE_OF:
            retval.Reset(new CSeq_loc);
            retval->SetEquiv();
            break;

            /* ERROR */
        case GBPARSE_INT_STRING:
            xgbparse_error("string in loc",
                           head_token, current_token);
            keep_rawPt = true;
            ++num_errPt;
            goto FATAL;
            /*--- no break on purpose---*/
        default:
        case  GBPARSE_INT_UNKNOWN:
        case  GBPARSE_INT_RIGHT:
        case  GBPARSE_INT_DOT_DOT:
        case  GBPARSE_INT_COMMA:
        case  GBPARSE_INT_SINGLE_DOT:
            xgbparse_error("illegal initial loc token",
                           head_token, currentPt);
            keep_rawPt = true;
            ++num_errPt;
            goto FATAL;

            /* Interval, occurs on recursion */
        case GBPARSE_INT_GAP:
            xgbgap(currentPt, end_it, retval, false);
            break;
        case GBPARSE_INT_UNK_GAP:
            xgbgap(currentPt, end_it, retval, true);
            break;

        case  GBPARSE_INT_ACCESSION:
        case  GBPARSE_INT_CARET:
        case  GBPARSE_INT_GT:
        case  GBPARSE_INT_LT:
        case  GBPARSE_INT_NUMBER:
        case  GBPARSE_INT_LEFT:
        case GBPARSE_INT_ONE_OF_NUM:
            retval = xgbint_ver(keep_rawPt, currentPt, head_token, end_it, num_errPt, seq_ids, accver);
            break;

        case  GBPARSE_INT_REPLACE:
            /*-------illegal at this level --*/
            xgbparse_error("illegal replace",
                           head_token, currentPt);
            keep_rawPt = true;
            ++num_errPt;
            goto FATAL;
        case GBPARSE_INT_SITES:
            sitesPt = true;
            go_again = true;
            ++currentPt;
            break;
        }
    } while (go_again && currentPt != end_it);

    if (!num_errPt)
    {
        if (retval.NotEmpty() && !retval->IsNull())
        {
            if (!retval->IsInt() && !retval->IsPnt()
                && !did_complement)
            {
                /*--------
                * ONLY THE CHOICE has been set. the "join", etc. only has been noted
                *----*/
                ++currentPt;
                if (currentPt == end_it)
                {
                    xgbparse_error("unexpected end of interval tokens",
                                   head_token, currentPt);
                    keep_rawPt = true;
                    ++num_errPt;
                    goto FATAL;
                }
                else
                {
                    if (currentPt->choice != GBPARSE_INT_LEFT)
                    {
                        xgbparse_error("Missing \'(\'",
                                       head_token, currentPt); /* paran match  ) */
                        keep_rawPt = true;
                        ++num_errPt;
                        goto FATAL;
                    }
                    else{
                        ++parenPt;
                        ++currentPt;
                        if (currentPt == end_it)
                        {
                            xgbparse_error("illegal null contents",
                                           head_token, currentPt);
                            keep_rawPt = true;
                            ++num_errPt;
                            goto FATAL;
                        }
                        else
                        {
                            if (currentPt->choice == GBPARSE_INT_RIGHT)
                            { /* paran match ( */
                                xgbparse_error("Premature \')\'",
                                               head_token, currentPt);
                                keep_rawPt = true;
                                ++num_errPt;
                                goto FATAL;
                            }
                            else
                            {
                                while (!num_errPt && currentPt != end_it)
                                {
                                    if (currentPt->choice == GBPARSE_INT_RIGHT)
                                    {
                                        while (currentPt->choice == GBPARSE_INT_RIGHT)
                                        {
                                            parenPt--;
                                            ++currentPt;
                                            if (currentPt == end_it)
                                                break;
                                        }
                                        break;
                                    }

                                    if (currentPt == end_it)
                                        break;

                                    CRef<CSeq_loc> next_loc = xgbloc_ver(keep_rawPt, parenPt, sitesPt,
                                                                                              currentPt, head_token, end_it, num_errPt,
                                                                                              seq_ids, accver);

                                    if (next_loc.NotEmpty())
                                    {
                                        if (retval->IsMix())
                                            retval->SetMix().AddSeqLoc(*next_loc);
                                        else // equiv
                                            retval->SetEquiv().Add(*next_loc);
                                    }

                                    if (currentPt == end_it || currentPt->choice == GBPARSE_INT_RIGHT)
                                        break;

                                    if (currentPt->choice == GBPARSE_INT_COMMA)
                                    {
                                        ++currentPt;
                                        if (add_nulls)
                                        {
                                            CRef<CSeq_loc> null_loc(new CSeq_loc);
                                            null_loc->SetNull();

                                            if (retval->IsMix())
                                                retval->SetMix().AddSeqLoc(*null_loc);
                                            else // equiv
                                                retval->SetEquiv().Add(*null_loc);
                                        }
                                    }
                                    else{
                                        xgbparse_error("Illegal token after interval",
                                                       head_token, currentPt);
                                        keep_rawPt = true;
                                        ++num_errPt;
                                        goto FATAL;
                                    }
                                }
                            }
                        }
                        if (currentPt == end_it)
                        {
                            xgbparse_error("unexpected end of usable tokens",
                                           head_token, currentPt);
                            keep_rawPt = true;
                            ++num_errPt;
                            goto FATAL;
                        }
                        else
                        {
                            if (currentPt->choice != GBPARSE_INT_RIGHT)
                            {
                                xgbparse_error("Missing \')\'" /* paran match  ) */,
                                               head_token, currentPt);
                                keep_rawPt = true;
                                ++num_errPt;
                                goto FATAL;
                            }
                            else
                            {
                                parenPt--;
                                ++currentPt;
                            }
                        }
                    }
                }
            }
        }
    }

FATAL:
    if (num_errPt)
    {
        if (retval.NotEmpty())
        {
            retval->Reset();
            retval->SetWhole().Assign(*(*seq_ids.begin()));
        }
    }

    return retval;
}




static CRef<CSeq_loc> xgbreplace_ver(bool& keep_rawPt, int& parenPt,
        bool& sitesPt, TTokenIt& currentPt,
        TTokenIt head_token, 
        TTokenIt end_it,
        int& num_errPt,
        const TSeqIdList& seq_ids, bool accver)
{
    CRef<CSeq_loc> ret;

    keep_rawPt = true;
    ++currentPt;

    if (currentPt->choice == GBPARSE_INT_LEFT)
    {
        ++currentPt;
        ret = xgbloc_ver(keep_rawPt, parenPt, sitesPt, currentPt, head_token, end_it,
                         num_errPt, seq_ids, accver);

        if (currentPt == end_it)
        {
            xgbparse_error("unexpected end of interval tokens",
                           head_token, currentPt);
            keep_rawPt = true;
            ++num_errPt;
        }
        else
        {

            if (currentPt->choice != GBPARSE_INT_COMMA)
            {
                xgbparse_error("Missing comma after first location in replace",
                               head_token, currentPt);
                ++num_errPt;
            }
        }
    }
    else
    {
        xgbparse_error("Missing \'(\'" /* paran match  ) */
                       , head_token, currentPt);
        ++num_errPt;
    }

    return ret;
}




/*---------- xgbparseint_ver()-----*/

CRef<CSeq_loc> xgbparseint_ver(const char* raw_intervals, bool& keep_rawPt, bool& sitesPt, int& num_errsPt,
                                                    const TSeqIdList& seq_ids, bool accver)
{


    keep_rawPt = false;
    sitesPt = false;


    TTokens tokens;
    num_errsPt = xgbparselex_ver(raw_intervals, tokens, accver);

    if (tokens.empty())
    {
        num_errsPt = 1;
        return Ref<CSeq_loc>();
    }

    CRef<CSeq_loc> ret;
    if ( !num_errsPt ) {
        xfind_one_of_num(tokens);
        auto head_token = tokens.begin();
        auto current_token = head_token;
        auto end_it = tokens.end();
    
        int paren_count = 0;
        bool go_again;
        do
        {
            go_again = false;
            if (current_token != end_it)
            {
                switch (current_token->choice)
                {
                case GBPARSE_INT_JOIN:
                case GBPARSE_INT_ORDER:
                case GBPARSE_INT_GROUP:
                case GBPARSE_INT_ONE_OF:
                case GBPARSE_INT_COMPL:
                    ret = xgbloc_ver(keep_rawPt, paren_count, sitesPt, current_token,
                                     head_token, end_it, num_errsPt, seq_ids, accver);
                    /* need to check that out of tokens here */
                    xgbparse_better_be_done(num_errsPt, current_token, head_token, end_it, keep_rawPt, paren_count);
                    break;

                case GBPARSE_INT_STRING:
                    xgbparse_error("string in loc", head_token, current_token);
                    keep_rawPt = true;
                    ++num_errsPt;
                    /*  no break on purpose */
                case  GBPARSE_INT_UNKNOWN:
                default:
                case  GBPARSE_INT_RIGHT:
                case  GBPARSE_INT_DOT_DOT:
                case  GBPARSE_INT_COMMA:
                case  GBPARSE_INT_SINGLE_DOT:

                    xgbparse_error("illegal initial token", head_token, current_token);
                    keep_rawPt = true;
                    ++num_errsPt;
                    ++current_token;
                    break;

                case  GBPARSE_INT_ACCESSION:
                    /*--- no warn, but strange ---*/
                    /*-- no break on purpose ---*/

                case  GBPARSE_INT_CARET: case  GBPARSE_INT_GT:
                case  GBPARSE_INT_LT: case  GBPARSE_INT_NUMBER:
                case  GBPARSE_INT_LEFT:

                case GBPARSE_INT_ONE_OF_NUM:

                    ret = xgbint_ver(keep_rawPt, current_token, head_token, end_it, num_errsPt, seq_ids, accver);

                    /* need to check that out of tokens here */
                    xgbparse_better_be_done(num_errsPt, current_token, head_token, end_it, keep_rawPt, paren_count);
                    break;

                case  GBPARSE_INT_REPLACE:
                    ret = xgbreplace_ver(keep_rawPt, paren_count, sitesPt, current_token,
                                         head_token, end_it, num_errsPt, seq_ids, accver);
                    keep_rawPt = true;
                    /*---all errors handled within this function ---*/
                    break;
                case GBPARSE_INT_SITES:
                    sitesPt = true;
                    go_again = true;
                    ++current_token;
                    break;
                }
            }
        } while (go_again && current_token != end_it);
    }
    else
    {
        keep_rawPt = true;
    }
    

    if (num_errsPt)
        ret.Reset();

    return ret;
}

END_NCBI_SCOPE
