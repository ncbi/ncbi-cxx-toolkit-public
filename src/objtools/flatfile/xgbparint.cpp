/* gbparint.c
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
* File Name:  gbparint.c
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
#include "valnode.h"
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
#define GBPARSE_INT_ACCESION 7
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

const Char* seqlitdbtag = "SeqLit";
const Char* unkseqlitdbtag = "UnkSeqLit";

/*--------- do_xgbparse_error () ---------------*/

#define ERR_FEATURE_LocationParsing_validatr 1,5

static void do_xgbparse_error (const Char* msg, const Char* details)
{
    size_t len = StringLen(msg) +7;
    char* errmsg;
    char* temp;

    len += StringLen(details);
    temp = errmsg = static_cast<char*>(MemNew((size_t)len));
    temp = StringMove(temp, msg);
    temp = StringMove(temp, " at ");
    temp = StringMove(temp, details);

    Nlm_ErrSetContext("validatr", __FILE__, __LINE__);
    Nlm_ErrPostEx(SEV_ERROR, ERR_FEATURE_LocationParsing_validatr, errmsg);

    MemFree(errmsg);
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

static char* xgbparse_point(ValNodePtr head, ValNodePtr current)
{
    char* temp;
    char* retval = 0;
    size_t len = 0;
    ValNodePtr now;

    for (now = head; now; now = now->next){
        switch (now->choice){
        case GBPARSE_INT_JOIN:
            len += 4;
            break;
        case GBPARSE_INT_COMPL:
            len += 10;
            break;
        case GBPARSE_INT_LEFT:
        case GBPARSE_INT_RIGHT:
        case GBPARSE_INT_CARET:
        case GBPARSE_INT_GT:
        case GBPARSE_INT_LT:
        case GBPARSE_INT_COMMA:
        case GBPARSE_INT_SINGLE_DOT:
            len++;
            break;
        case GBPARSE_INT_DOT_DOT:
            len += 2;
            break;
        case GBPARSE_INT_ACCESION:
        case GBPARSE_INT_NUMBER:
            len += StringLen(static_cast<char*>(now->data.ptrvalue));
            break;
        case GBPARSE_INT_ORDER:
        case GBPARSE_INT_GROUP:
            len += 5;
            break;
        case GBPARSE_INT_ONE_OF:
        case GBPARSE_INT_ONE_OF_NUM:
            len += 6;
            break;
        case GBPARSE_INT_REPLACE:
            len += 7;
            break;
        case GBPARSE_INT_STRING:
            len += StringLen(static_cast<char*>(now->data.ptrvalue)) + 1;
            break;
        case GBPARSE_INT_UNKNOWN:
        default:
            break;
        }
        len++; /* for space */


        if (now == current)
            break;
    }


    if (len > 0){
        temp = retval = static_cast<char*>(MemNew(len + 1));
        for (now = head; now; now = now->next){
            switch (now->choice){
            case GBPARSE_INT_JOIN:
                temp = StringMove(temp, "join");
                break;
            case GBPARSE_INT_COMPL:
                temp = StringMove(temp, "complement");
                break;
            case GBPARSE_INT_LEFT:
                temp = StringMove(temp, "(");
                break;
            case GBPARSE_INT_RIGHT:
                temp = StringMove(temp, ")");
                break;
            case GBPARSE_INT_CARET:
                temp = StringMove(temp, "^");
                break;
            case GBPARSE_INT_DOT_DOT:
                temp = StringMove(temp, "..");
                break;
            case GBPARSE_INT_ACCESION:
            case GBPARSE_INT_NUMBER:
            case GBPARSE_INT_STRING:
                temp = StringMove(temp, static_cast<char*>(now->data.ptrvalue));
                break;
            case GBPARSE_INT_GT:
                temp = StringMove(temp, ">");
                break;
            case GBPARSE_INT_LT:
                temp = StringMove(temp, "<");
                break;
            case GBPARSE_INT_COMMA:
                temp = StringMove(temp, ",");
                break;
            case GBPARSE_INT_ORDER:
                temp = StringMove(temp, "order");
                break;
            case GBPARSE_INT_SINGLE_DOT:
                temp = StringMove(temp, ".");
                break;
            case GBPARSE_INT_GROUP:
                temp = StringMove(temp, "group");
                break;
            case GBPARSE_INT_ONE_OF:
            case GBPARSE_INT_ONE_OF_NUM:
                temp = StringMove(temp, "one-of");
                break;
            case GBPARSE_INT_REPLACE:
                temp = StringMove(temp, "replace");
                break;
            case GBPARSE_INT_UNKNOWN:
            default:
                break;
            }
            temp = StringMove(temp, " ");
            if (now == current)
                break;
        }
    }

    return retval;
}
/*--------- xgbparse_error()-----------*/

static void xgbparse_error(const Char* front, ValNodePtr head, ValNodePtr current)
{
    char* details;

    details = xgbparse_point (head, current);
    Err_func (front,details); 
    MemFree(details);
}

/*------------------ xgbcheck_range()-------------*/
static void xgbcheck_range(TSeqPos num, const objects::CSeq_id& id, bool& keep_rawPt, int& num_errsPt, ValNodePtr head, ValNodePtr current)
{
    TSeqPos len;
    if (Range_func != NULL)
    {
        len = (*Range_func)(xgbparse_range_data, id);
        if (len != static_cast<TSeqPos>(-1))
        {
            if (num >= len)
            {
                xgbparse_error("range error", head, current);
                keep_rawPt = true;
                ++num_errsPt;
            }
        }
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

static void xfind_one_of_num(ValNodePtr head_token)
{
    ValNodePtr current, scanner;

    current = head_token;
    if (current -> choice == GBPARSE_INT_ONE_OF){
            scanner= current -> next;
/*-------(is first token after ")" a ".."?----*/
            for (;scanner!=NULL; scanner = scanner -> next){
                if (scanner -> choice == GBPARSE_INT_RIGHT){
                    scanner = scanner -> next;
                    if (scanner != NULL){
                        if (scanner -> choice == GBPARSE_INT_DOT_DOT){
/*---- this is it ! ! */
                            current -> choice = GBPARSE_INT_ONE_OF_NUM;
                        }
                    }
                    break;
                }
            }
    }
    for (current = head_token; current != NULL; current = current -> next){
        if ( current -> choice == GBPARSE_INT_COMMA || 
            current -> choice == GBPARSE_INT_LEFT ){
            scanner= current -> next;
            if ( scanner != NULL){
                if (scanner -> choice == GBPARSE_INT_ONE_OF){
/*-------(is first token after ")" a ".."?----*/
                    for (;scanner!=NULL; scanner = scanner -> next){
                        if (scanner -> choice == GBPARSE_INT_RIGHT){
                            scanner = scanner -> next;
                            if (scanner != NULL){
                                if (scanner -> choice == GBPARSE_INT_DOT_DOT){
/*---- this is it ! ! */
                                    current -> next -> choice 
                                        = GBPARSE_INT_ONE_OF_NUM;
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

}


/**********************************************************/
static size_t xgbparse_accprefix(char* acc)
{
    char* p;

    if (acc == NULL || *acc == '\0')
        return(0);

    for (p = acc; IS_ALPHA(*p) != 0;)
        p++;
    size_t ret = p - acc;
    if (*p == '_')
    {
        if (ret == 2)
        {
            for (p++; IS_ALPHA(*p) != 0;)
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

static char Saved_ch;

#define xlex_error_MACRO(msg)\
        if (current_col != NULL && *current_col){\
        Saved_ch = *(current_col +1);\
        *(current_col +1) = '\0';\
                }else{\
        Saved_ch='\0';\
                }\
        xgbparse_error(msg, & forerrmacro, & forerrmacro);\
        if (Saved_ch)\
        *(current_col +1) = Saved_ch;


/*------------- xgbparselex_ver() -----------------------*/

static int xgbparselex_ver(char* linein, ValNodePtr* lexed, bool accver)
{
    char* current_col = 0, *points_at_term_null, *spare, *line_use = 0;
    size_t dex = 0,
           retval = 0,
           len = 0;

    ValNodePtr current_token = NULL,
               last_token = NULL;

    bool skip_new_token = false;
    bool die_now = false;
    ValNode forerrmacro;

    forerrmacro.choice = GBPARSE_INT_ACCESION;

    if (*linein)
    {
        len = StringLen(linein);
        line_use = static_cast<char*>(MemNew(len + 1));
        StringCpy(line_use, linein);
        if (*lexed)
        {
            xlex_error_MACRO("Lex list not cleared on entry to Nlm_gbparselex_ver")
                ValNodeFree(*lexed);
            *lexed = NULL;
        }
        current_col = line_use;
        forerrmacro.data.ptrvalue = line_use;

        /*---------
        *   Clear terminal white space
        *---------*/
        points_at_term_null = line_use + len;
        spare = points_at_term_null - 1;
        while (*spare == ' ' || *spare == '\n' || *spare == '\r' || *spare == '~') {
            *spare-- = '\0';
            points_at_term_null--;
        }


        while (current_col < points_at_term_null && !die_now) {
            if (!skip_new_token){
                last_token = current_token;
                current_token = ValNodeNew(current_token);
                if (!* lexed)
                    * lexed = current_token;
            }
            switch (*current_col){

            case '\"':
                skip_new_token = false;
                current_token->choice = GBPARSE_INT_STRING;
                for (spare = current_col + 1; spare < points_at_term_null;
                     spare++) {
                    if (*spare == '\"'){
                        break;
                    }
                }
                if (spare >= points_at_term_null){
                    xlex_error_MACRO("unterminated string")
                        retval++;
                }
                else{
                    len = spare - current_col + 1;
                    current_token->data.ptrvalue =
                        MemNew(len + 2);
                    StringNCpy(static_cast<char*>(current_token->data.ptrvalue),
                               current_col, len);
                    current_col += len;
                }
                break;
                /*------
                *  NUMBER
                *------*/
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                skip_new_token = false;
                current_token->choice = GBPARSE_INT_NUMBER;
                for (dex = 0, spare = current_col; isdigit((int)*spare); spare++){
                    dex++;
                }
                current_token->data.ptrvalue = MemNew(dex + 1);
                StringNCpy(static_cast<char*>(current_token->data.ptrvalue), current_col, dex);
                current_col += dex - 1;
                break;
                /*------
                *  JOIN
                *------*/
            case 'j':
                skip_new_token = false;
                current_token->choice = GBPARSE_INT_JOIN;
                if (StringNCmp(current_col, "join", (unsigned)4) != 0){
                    xlex_error_MACRO("\"join\" misspelled")
                        retval += 10;
                    for (; *current_col && *current_col != '('; current_col++)
                        ; /* vi match )   empty body*/
                    current_col--;  /* back up 'cause ++ follows */
                }
                else{
                    current_col += 3;
                }
                break;

                /*------
                *  ORDER and ONE-OF
                *------*/
            case 'o':
                skip_new_token = false;
                if (StringNCmp(current_col, "order", (unsigned)5) != 0){
                    if (StringNCmp(current_col, "one-of", (unsigned)6) != 0){
                        xlex_error_MACRO("\"order\" or \"one-of\" misspelled")
                            retval++;
                        for (; *current_col && *current_col != '('; current_col++)
                            ; /* vi match )   empty body*/
                        current_col--;  /* back up 'cause ++ follows */
                    }
                    else{
                        current_token->choice = GBPARSE_INT_ONE_OF;
                        current_col += 5;
                    }
                }
                else{
                    current_token->choice = GBPARSE_INT_ORDER;
                    current_col += 4;
                }
                break;

                /*------
                *  REPLACE
                *------*/
            case 'r':
                skip_new_token = false;
                current_token->choice = GBPARSE_INT_REPLACE;
                if (StringNCmp(current_col, "replace", (unsigned)6) != 0){
                    xlex_error_MACRO("\"replace\" misspelled")
                        retval++;
                    for (; *current_col && *current_col != '('; current_col++)
                        ; /* vi match )   empty body*/
                    current_col--;  /* back up 'cause ++ follows */
                }
                else{
                    current_col += 6;
                }
                break;

                /*------
                *  GAP or GROUP or GI
                *------*/
            case 'g':
                skip_new_token = false;
                if (StringNCmp(current_col, "gap", 3) == 0 &&
                    (current_col[3] == '(' ||
                    current_col[3] == ' ' ||
                    current_col[3] == '\t' ||
                    current_col[3] == '\0'))
                {
                    current_token->choice = GBPARSE_INT_GAP;
                    current_token->data.ptrvalue = MemNew(4);
                    StringCpy(static_cast<char*>(current_token->data.ptrvalue), "gap");
                    if (StringNICmp(current_col + 3, "(unk", 4) == 0)
                    {
                        current_token->choice = GBPARSE_INT_UNK_GAP;
                        last_token = current_token;
                        current_token = ValNodeNew(current_token);
                        current_token->choice = GBPARSE_INT_LEFT;
                        current_col += 4;
                    }
                    current_col += 2;
                    break;
                }
                if (StringNCmp(current_col, "gi|", 3) == 0) {
                    current_token->choice = GBPARSE_INT_ACCESION;
                    current_col += 3;
                    for (; IS_DIGIT(*current_col); current_col++);
                    break;
                }
                current_token->choice = GBPARSE_INT_GROUP;
                if (StringNCmp(current_col, "group", (unsigned)5) != 0){
                    xlex_error_MACRO("\"group\" misspelled")
                        retval++;
                    for (; *current_col && *current_col != '('; current_col++)
                        ; /* vi match )   empty body*/
                    current_col--;  /* back up 'cause ++ follows */
                }
                else{
                    current_col += 4;
                }
                break;

                /*------
                *  COMPLEMENT
                *------*/
            case 'c':
                skip_new_token = false;
                current_token->choice = GBPARSE_INT_COMPL;
                if (StringNCmp(current_col, "complement", (unsigned)10) != 0){
                    xlex_error_MACRO("\"complement\" misspelled")
                        retval += 10;
                    for (; *current_col && *current_col != '('; current_col++)
                        ; /* vi match )   empty body*/
                    current_col--;  /* back up 'cause ++ follows */
                }
                else{
                    current_col += 9;
                }
                break;

                /*-------
                * internal bases ignored
                *---------*/
            case 'b':
                if (StringNCmp(current_col, "bases", (unsigned)5) != 0){
                    goto ACCESSION;
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
                if (StringNCmp(current_col, "(base", (unsigned)5) == 0){
                    skip_new_token = false;
                    current_token->choice = GBPARSE_INT_JOIN;
                    current_col += 4;
                    if (*current_col != '\0')
                        if (*(current_col + 1) == 's')
                            current_col++;
                    last_token = current_token;
                    current_token = ValNodeNew(current_token);
                    current_token->choice = GBPARSE_INT_LEFT;
                }
                else if (StringNCmp(current_col, "(sites", (unsigned)5) == 0){
                    skip_new_token = false;
                    current_col += 5;
                    if (*current_col != '\0')
                    {
                        if (*(current_col + 1) == ')'){
                            current_col++;
                            current_token->choice = GBPARSE_INT_SITES;
                        }
                        else{
                            current_token->choice = GBPARSE_INT_SITES;
                            last_token = current_token;
                            current_token = ValNodeNew(current_token);
                            current_token->choice = GBPARSE_INT_JOIN;
                            last_token = current_token;
                            current_token = ValNodeNew(current_token);
                            current_token->choice = GBPARSE_INT_LEFT;
                            if (*current_col != '\0'){
                                if (*(current_col + 1) == ';'){
                                    current_col++;
                                }
                                else if (StringNCmp(current_col + 1, " ;", (unsigned)2) == 0){
                                    current_col += 2;
                                }
                            }
                        }
                    }
                }
                else{
                    skip_new_token = false;
                    current_token->choice = GBPARSE_INT_LEFT;
                }
                break;

            case ')':
                skip_new_token = false;
                current_token->choice = GBPARSE_INT_RIGHT;

                break;

            case '^':
                skip_new_token = false;
                current_token->choice = GBPARSE_INT_CARET;
                break;

            case '-':
                skip_new_token = false;
                current_token->choice = GBPARSE_INT_DOT_DOT;
                break;
            case '.':
                skip_new_token = false;
                if (StringNCmp(current_col, "..", (unsigned)2) != 0){
                    current_token->choice = GBPARSE_INT_SINGLE_DOT;
                }
                else{
                    current_token->choice = GBPARSE_INT_DOT_DOT;
                    current_col++;
                }
                break;

            case '>':
                skip_new_token = false;
                current_token->choice = GBPARSE_INT_GT;
                break;

            case '<':
                skip_new_token = false;
                current_token->choice = GBPARSE_INT_LT;

                break;

            case ';':
            case ',':
                skip_new_token = false;
                current_token->choice = GBPARSE_INT_COMMA;
                break;

            case ' ': case '\t': case '\n': case '\r': case '~':
                skip_new_token = true;
                break;

            case 't':
                if (StringNCmp(current_col, "to", (unsigned)2) != 0){
                    goto ACCESSION;
                }
                else{
                    skip_new_token = false;
                    current_token->choice = GBPARSE_INT_DOT_DOT;
                    current_col++;
                    break;
                }

            case 's':
                if (StringNCmp(current_col, "site", (unsigned)4) != 0){
                    goto ACCESSION;
                }
                else{
                    skip_new_token = false;
                    current_token->choice = GBPARSE_INT_SITES;
                    current_col += 3;
                    if (*current_col != '\0')
                        if (*(current_col + 1) == 's')
                            current_col++;
                    if (*current_col != '\0'){
                        if (*(current_col + 1) == ';'){
                            current_col++;
                        }
                        else if (StringNCmp(current_col + 1, " ;", (unsigned)2) == 0){
                            current_col += 2;
                        }
                    }
                    break;
                }


            ACCESSION:
            default:
                /*-------
                * all GenBank accessions start with a capital letter
                * and then have numbers
                ------*/
                /* new accessions start with 2 capital letters !!  1997 */
                /* new accessions have .version !!  2/15/1999 */
                skip_new_token = false;
                current_token->choice = GBPARSE_INT_ACCESION;
                dex = xgbparse_accprefix(current_col);
                spare = current_col + dex;
                for (; isdigit((int)*spare); spare++){
                    dex++;
                }
                if (accver && *spare == '.') {
                    dex++;
                    for (spare++; isdigit((int)*spare); spare++){
                        dex++;
                    }
                }
                if (*spare != ':'){
                    xlex_error_MACRO("ACCESSION missing \":\"")
                        retval += 10;
                    current_col--;
                }
                current_token->data.ptrvalue = MemNew(dex + 1);
                StringNCpy(static_cast<char*>(current_token->data.ptrvalue), current_col, dex);
                current_col += dex;


            }
            /*--move to past last "good" character---*/
            current_col++;
        }
        if (!* lexed && current_token){
            *lexed = current_token;
        }
        if (skip_new_token && current_token) {
            /*---------
            *   last node points to a null (blank or white space token)
            *-----------*/
            if (last_token){
                last_token->next = NULL;
            }
            else{
                *lexed = NULL;
            }
            ValNodeFree(current_token);
        }
    }
    if (line_use)
        MemFree(line_use);

    return static_cast<int>(retval);
}

/*----------------- xgbparse_better_be_done()-------------*/
static void xgbparse_better_be_done(int& num_errsPt, ValNodePtr current_token, ValNodePtr head_token, bool& keep_rawPt, int paren_count)
{
    if (current_token)
    {
        while (current_token->choice == GBPARSE_INT_RIGHT)
        {
            paren_count--;
            current_token = current_token->next;
            if (!current_token)
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

    if (current_token)
    {
        xgbparse_error("text after end",
                       head_token, current_token);
        keep_rawPt = true;
        ++num_errsPt;
    }
}

/**********************************************************
*
*   CRef<objects::CSeq_loc> XGapToSeqLocEx(range, unknown):
*
*      Gets the size of gap and constructs SeqLoc block with
*   $(seqlitdbtag) value as Dbtag.db and Dbtag.tag.id = 0.
*
**********************************************************/
static CRef<objects::CSeq_loc> XGapToSeqLocEx(Int4 range, bool unknown)
{
    CRef<objects::CSeq_loc> ret;

    if (range < 0)
        return ret;

    ret.Reset(new objects::CSeq_loc);
    if (range == 0)
    {
        ret->SetNull();
        return ret;
    }

    objects::CSeq_interval& interval = ret->SetInt();
    interval.SetFrom(0);
    interval.SetTo(range - 1);

    objects::CSeq_id& id = interval.SetId();
    id.SetGeneral().SetDb(unknown ? unkseqlitdbtag : seqlitdbtag);
    id.SetGeneral().SetTag().SetId(0);

    return ret;
}

/**********************************************************/
static void xgbgap(ValNodePtr& currentPt, CRef<objects::CSeq_loc>& loc, bool unknown)
{
    ValNodePtr vnp_first;
    ValNodePtr vnp_second;
    ValNodePtr vnp_third;

    vnp_first = currentPt->next;
    if (vnp_first == NULL || vnp_first->choice != GBPARSE_INT_LEFT)
        return;

    vnp_second = vnp_first->next;
    if (vnp_second == NULL || (vnp_second->choice != GBPARSE_INT_NUMBER &&
        vnp_second->choice != GBPARSE_INT_RIGHT))
        return;

    if (vnp_second->choice == GBPARSE_INT_RIGHT)
    {
        loc->SetNull();
    }
    else
    {
        vnp_third = vnp_second->next;
        if (vnp_third == NULL || vnp_third->choice != GBPARSE_INT_RIGHT)
            return;

        CRef<objects::CSeq_loc> new_loc = XGapToSeqLocEx(atoi((char*)vnp_second->data.ptrvalue), unknown);
        if (new_loc.Empty())
            return;

        currentPt = currentPt->next;
        loc = new_loc;
    }

    currentPt = currentPt->next;
    currentPt = currentPt->next;
    currentPt = currentPt->next;
}

/*------------------- xgbpintpnt()-----------*/

static void xgbpintpnt(objects::CSeq_loc& loc)
{
    CRef<objects::CSeq_point> point(new objects::CSeq_point);

    point->SetPoint(loc.GetInt().GetFrom());

    if (loc.GetInt().IsSetId())
        point->SetId(loc.SetInt().SetId());

    if (loc.GetInt().IsSetFuzz_from())
        point->SetFuzz(loc.SetInt().SetFuzz_from());

    loc.SetPnt(*point);
}

/*----- xgbload_number() -----*/

static void xgbload_number(TSeqPos& numPt, objects::CInt_fuzz& fuzz, bool& keep_rawPt, ValNodePtr& currentPt, ValNodePtr head_token, int& num_errPt, int take_which)
{
    int num_found = 0;
    int fuzz_err = 0;
    bool strange_sin_dot = false;

    if (currentPt->choice == GBPARSE_INT_CARET)
    {
        xgbparse_error("duplicate carets", head_token, currentPt);
        keep_rawPt = true;
        ++num_errPt;
        currentPt = currentPt->next;
        fuzz_err = 1;
    }
    else if (currentPt->choice == GBPARSE_INT_GT ||
             currentPt->choice == GBPARSE_INT_LT)
    {
        if (currentPt->choice == GBPARSE_INT_GT)
            fuzz.SetLim(objects::CInt_fuzz::eLim_gt);
        else
            fuzz.SetLim(objects::CInt_fuzz::eLim_lt);

        currentPt = currentPt->next;
    }
    else if (currentPt->choice == GBPARSE_INT_LEFT)
    {
        strange_sin_dot = true;
        currentPt = currentPt->next;
        fuzz.SetRange();

        if (currentPt->choice == GBPARSE_INT_NUMBER)
        {
            fuzz.SetRange().SetMin(atoi(static_cast<char*>(currentPt->data.ptrvalue)) - 1);
            if (take_which == TAKE_FIRST)
            {
                numPt = fuzz.GetRange().GetMin();
            }
            currentPt = currentPt->next;
            num_found = 1;
        }
        else
            fuzz_err = 1;

        if (currentPt->choice != GBPARSE_INT_SINGLE_DOT)
            fuzz_err = 1;
        else
        {
            currentPt = currentPt->next;
            if (currentPt->choice == GBPARSE_INT_NUMBER)
            {
                fuzz.SetRange().SetMax(atoi(static_cast<char*>(currentPt->data.ptrvalue)) - 1);
                if (take_which == TAKE_SECOND)
                {
                    numPt = fuzz.GetRange().GetMax();
                }
                currentPt = currentPt->next;
            }
            else
                fuzz_err = 1;

            if (currentPt->choice == GBPARSE_INT_RIGHT)
                currentPt = currentPt->next;
            else
                fuzz_err = 1;
        }

    }
    else if (currentPt->choice != GBPARSE_INT_NUMBER)
    {
        /* this prevents endless cycling, unconditionally */
        if (currentPt->choice != GBPARSE_INT_ONE_OF
            && currentPt->choice != GBPARSE_INT_ONE_OF_NUM)
            currentPt = currentPt->next;
        num_found = -1;
    }

    if (!strange_sin_dot)
    {
        if (!currentPt)
        {
            xgbparse_error("unexpected end of interval tokens",
                           head_token, currentPt);
            keep_rawPt = true;
            ++num_errPt;
        }
        else{
            if (currentPt->choice == GBPARSE_INT_NUMBER)
            {
                numPt = atoi(static_cast<char*>(currentPt->data.ptrvalue)) - 1;
                currentPt = currentPt->next;
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

            currentPt = currentPt->next;
            if (currentPt->choice != GBPARSE_INT_LEFT)
            {
                one_of_ok = false;
            }
            else
            {
                currentPt = currentPt->next;
            }

            if (one_of_ok && currentPt->choice == GBPARSE_INT_NUMBER)
            {
                numPt = atoi(static_cast<char*>(currentPt->data.ptrvalue)) - 1;
                currentPt = currentPt->next;
            }
            else
            {
                one_of_ok = false;
            }

            while (one_of_ok && !at_end_one_of &&  currentPt != NULL)
            {
                switch (currentPt->choice)
                {
                default:
                    one_of_ok = false;
                    break;
                case GBPARSE_INT_COMMA:
                case GBPARSE_INT_NUMBER:
                    currentPt = currentPt->next;
                    break;
                case GBPARSE_INT_RIGHT:
                    currentPt = currentPt->next;
                    at_end_one_of = true;
                    break;
                }
            }

            if (!one_of_ok && !at_end_one_of)
            {
                while (!at_end_one_of && currentPt != NULL)
                {
                    if (currentPt->choice == GBPARSE_INT_RIGHT)
                        at_end_one_of = true;
                    currentPt = currentPt->next;
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
static CRef<objects::CSeq_loc> xgbint_ver(bool& keep_rawPt, ValNodePtr& currentPt,
                                                      ValNodePtr head_token, int& num_errPt, const TSeqIdList& seq_ids,
                                                      bool accver)
{
    CRef<objects::CSeq_loc> ret(new objects::CSeq_loc);

    bool took_choice = false;
    char* p;

    CRef<objects::CSeq_id> new_id;
    CRef<objects::CInt_fuzz> new_fuzz;

    if (currentPt->choice == GBPARSE_INT_ACCESION)
    {
        CRef<objects::CTextseq_id> text_id(new objects::CTextseq_id);

        if (accver == false)
        {
            text_id->SetAccession(static_cast<char*>(currentPt->data.ptrvalue));
        }
        else
        {
            p = StringChr(static_cast<char*>(currentPt->data.ptrvalue), '.');
            if (p == NULL)
            {
                text_id->SetAccession(static_cast<char*>(currentPt->data.ptrvalue));
                xgbparse_error("Missing accession's version",
                               head_token, currentPt);
            }
            else
            {
                *p = '\0';
                text_id->SetAccession(static_cast<char*>(currentPt->data.ptrvalue));
                text_id->SetVersion(atoi(p + 1));
                *p = '.';
            }
        }

        new_id.Reset(new objects::CSeq_id);
        if (!seq_ids.empty())
        {
            const objects::CSeq_id& first_id = *(*seq_ids.begin());
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

        currentPt = currentPt->next;
        if (!currentPt)
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
        new_id.Reset(new ncbi::objects::CSeq_id);
        new_id->Assign(*(*seq_ids.begin()));
    }

    if (currentPt->choice == GBPARSE_INT_LT)
    {
        new_fuzz.Reset(new objects::CInt_fuzz);
        new_fuzz->SetLim(objects::CInt_fuzz::eLim_lt);

        currentPt = currentPt->next;
        if (!currentPt)
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
        case  GBPARSE_INT_ACCESION:
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
                           keep_rawPt, currentPt, head_token,
                           num_errPt, TAKE_FIRST);

            if (ret->GetInt().GetFuzz_from().Which() == objects::CInt_fuzz::e_not_set)
                ret->SetInt().ResetFuzz_from();

            xgbcheck_range(ret->GetInt().GetFrom(), *new_id, keep_rawPt, num_errPt, head_token, currentPt);

            if (!num_errPt)
            {
                if (currentPt)
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
                    case GBPARSE_INT_ACCESION:
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

                        ret->SetInt().SetFuzz_from().SetLim(objects::CInt_fuzz::eLim_tl);
                        ret->SetInt().SetFuzz_to().SetLim(objects::CInt_fuzz::eLim_tl);
                        in_caret = true;
                        /*---no break on purpose ---*/

                    case GBPARSE_INT_DOT_DOT:
                        currentPt = currentPt->next;
                        if (currentPt == NULL)
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
                                       keep_rawPt, currentPt, head_token,
                                       num_errPt, TAKE_SECOND);
                        if (ret->GetInt().GetFuzz_to().Which() == objects::CInt_fuzz::e_not_set)
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
                            objects::CSeq_point& point = ret->SetPnt();
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


/*---------- xgbloc_ver()-----*/

static CRef<objects::CSeq_loc> xgbloc_ver(bool& keep_rawPt, int& parenPt,
                                                      bool& sitesPt, ValNodePtr& currentPt,
                                                      ValNodePtr head_token, int& num_errPt,
                                                      const TSeqIdList& seq_ids, bool accver)
{
    CRef<objects::CSeq_loc> retval;

    bool add_nulls = false;
    ValNodePtr current_token = currentPt;
    bool did_complement = false;
    bool go_again;

    do
    {
        go_again = false;
        switch (current_token->choice)
        {
        case  GBPARSE_INT_COMPL:
            currentPt = currentPt->next;
            if (currentPt == NULL){
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
                ++parenPt; currentPt = currentPt->next;
                if (!currentPt){
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
                                            head_token, num_errPt, seq_ids, accver);

                        if (retval.NotEmpty())
                            retval = objects::sequence::SeqLocRevCmpl(*retval, nullptr);

                        did_complement = true;
                        if (currentPt){
                            if (currentPt->choice != GBPARSE_INT_RIGHT){
                                xgbparse_error("Missing \')\'",
                                               head_token, currentPt);
                                keep_rawPt = true;
                                ++num_errPt;
                                goto FATAL;
                            }
                            else{
                                --parenPt;
                                currentPt = currentPt->next;
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
            retval.Reset(new objects::CSeq_loc);
            retval->SetMix();
            break;
        case  GBPARSE_INT_ORDER:
            retval.Reset(new objects::CSeq_loc);
            retval->SetMix();
            add_nulls = true;
            break;
        case  GBPARSE_INT_GROUP:
            retval.Reset(new objects::CSeq_loc);
            retval->SetMix();
            keep_rawPt = true;
            break;
        case  GBPARSE_INT_ONE_OF:
            retval.Reset(new objects::CSeq_loc);
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
            xgbgap(currentPt, retval, false);
            break;
        case GBPARSE_INT_UNK_GAP:
            xgbgap(currentPt, retval, true);
            break;

        case  GBPARSE_INT_ACCESION:
        case  GBPARSE_INT_CARET:
        case  GBPARSE_INT_GT:
        case  GBPARSE_INT_LT:
        case  GBPARSE_INT_NUMBER:
        case  GBPARSE_INT_LEFT:

        case GBPARSE_INT_ONE_OF_NUM:

            retval = xgbint_ver(keep_rawPt, currentPt, head_token, num_errPt, seq_ids, accver);
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
            currentPt = currentPt->next;
            break;
        }
    } while (go_again && currentPt);

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
                currentPt = currentPt->next;
                if (!currentPt)
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
                        currentPt = currentPt->next;
                        if (!currentPt)
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
                                while (!num_errPt && currentPt)
                                {
                                    if (currentPt->choice == GBPARSE_INT_RIGHT)
                                    {
                                        while (currentPt->choice == GBPARSE_INT_RIGHT)
                                        {
                                            parenPt--;
                                            currentPt = currentPt->next;
                                            if (!currentPt)
                                                break;
                                        }
                                        break;
                                    }

                                    if (!currentPt)
                                        break;

                                    CRef<objects::CSeq_loc> next_loc = xgbloc_ver(keep_rawPt, parenPt, sitesPt,
                                                                                              currentPt, head_token, num_errPt,
                                                                                              seq_ids, accver);

                                    if (next_loc.NotEmpty())
                                    {
                                        if (retval->IsMix())
                                            retval->SetMix().AddSeqLoc(*next_loc);
                                        else // equiv
                                            retval->SetEquiv().Add(*next_loc);
                                    }

                                    if (!currentPt || currentPt->choice == GBPARSE_INT_RIGHT)
                                        break;

                                    if (currentPt->choice == GBPARSE_INT_COMMA)
                                    {
                                        currentPt = currentPt->next;
                                        if (add_nulls)
                                        {
                                            CRef<objects::CSeq_loc> null_loc(new objects::CSeq_loc);
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
                        if (currentPt == NULL)
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
                                currentPt = currentPt->next;
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

/*-------- xgbreplace_ver() --------*/

static CRef<objects::CSeq_loc> xgbreplace_ver(bool& keep_rawPt, int& parenPt,
                                                          bool& sitesPt, ValNodePtr& currentPt,
                                                          ValNodePtr head_token, int& num_errPt,
                                                          const TSeqIdList& seq_ids, bool accver)
{
    CRef<objects::CSeq_loc> ret;

    keep_rawPt = true;
    currentPt = currentPt->next;

    if (currentPt->choice == GBPARSE_INT_LEFT)
    {
        currentPt = currentPt->next;
        ret = xgbloc_ver(keep_rawPt, parenPt, sitesPt, currentPt, head_token,
                         num_errPt, seq_ids, accver);
        if (!currentPt)
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

CRef<objects::CSeq_loc> xgbparseint_ver(char* raw_intervals, bool& keep_rawPt, bool& sitesPt, int& num_errsPt,
                                                    const TSeqIdList& seq_ids, bool accver)
{
    CRef<objects::CSeq_loc> ret;

    int paren_count = 0;
    bool go_again = false;

    keep_rawPt = false;
    sitesPt = false;

    ValNodePtr head_token = NULL,
               current_token = NULL;

    num_errsPt = xgbparselex_ver(raw_intervals, &head_token, accver);

    if (head_token == NULL)
    {
        num_errsPt = 1;
        return ret;
    }

    if ( !num_errsPt)
    {
        current_token = head_token;
        xfind_one_of_num(head_token);

        do
        {
            go_again = false;
            if (current_token)
            {
                switch (current_token->choice)
                {
                case GBPARSE_INT_JOIN:
                case GBPARSE_INT_ORDER:
                case GBPARSE_INT_GROUP:
                case GBPARSE_INT_ONE_OF:
                case GBPARSE_INT_COMPL:
                    ret = xgbloc_ver(keep_rawPt, paren_count, sitesPt, current_token,
                                     head_token, num_errsPt, seq_ids, accver);
                    /* need to check that out of tokens here */
                    xgbparse_better_be_done(num_errsPt, current_token, head_token, keep_rawPt, paren_count);
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
                    current_token = current_token->next;
                    break;

                case  GBPARSE_INT_ACCESION:
                    /*--- no warn, but strange ---*/
                    /*-- no break on purpose ---*/

                case  GBPARSE_INT_CARET: case  GBPARSE_INT_GT:
                case  GBPARSE_INT_LT: case  GBPARSE_INT_NUMBER:
                case  GBPARSE_INT_LEFT:

                case GBPARSE_INT_ONE_OF_NUM:

                    ret = xgbint_ver(keep_rawPt, current_token, head_token, num_errsPt, seq_ids, accver);

                    /* need to check that out of tokens here */
                    xgbparse_better_be_done(num_errsPt, current_token, head_token, keep_rawPt, paren_count);
                    break;

                case  GBPARSE_INT_REPLACE:
                    ret = xgbreplace_ver(keep_rawPt, paren_count, sitesPt, current_token,
                                         head_token, num_errsPt, seq_ids, accver);
                    keep_rawPt = true;
                    /*---all errors handled within this function ---*/
                    break;
                case GBPARSE_INT_SITES:
                    sitesPt = true;
                    go_again = true;
                    current_token = current_token->next;
                    break;
                }
            }
        } while (go_again && current_token);
    }
    else
    {
        keep_rawPt = true;
    }
    
    if ( head_token)
        ValNodeFreeData(head_token);

    if (num_errsPt)
        ret.Reset();

    return ret;
}

END_NCBI_SCOPE
