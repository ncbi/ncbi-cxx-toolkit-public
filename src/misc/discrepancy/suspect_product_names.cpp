/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Authors: Sema
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbimisc.hpp>
#include "discrepancy_core.hpp"
#include "utils.hpp"
#include <objects/macro/Field_type.hpp>
#include <objects/macro/Field_constraint.hpp>
#include <objects/macro/Location_pos_constraint.hpp>
#include <objects/macro/Match_type_constraint_.hpp>
#include <objects/macro/Misc_field_.hpp>
#include <objects/macro/Molinfo_field_constraint.hpp>
#include <objects/macro/Publication_constraint.hpp>
#include <objects/macro/Seque_const_mol_type_const.hpp>
#include <objects/macro/Sequence_constraint.hpp>
#include <objects/macro/Source_constraint.hpp>
#include <objects/macro/String_constraint_set.hpp>
#include <objects/macro/Translation_constraint.hpp>
#include <objects/macro/Word_substitution.hpp>
#include <objects/macro/Word_substitution_set.hpp>
#include <objects/macro/Suspect_rule_set.hpp>
#include <objects/macro/Suspect_rule.hpp>
#include <objects/macro/Search_func.hpp>
#include <objects/macro/Constraint_choice.hpp>
#include <sstream>

BEGIN_NCBI_SCOPE;
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(suspect_product_names);

static const string skip_bracket_paren[] = {
    "(NAD(P)H)",
    "(NAD(P))",
    "(I)",
    "(II)",
    "(III)",
    "(NADPH)",
    "(NAD+)",
    "(NAPPH/NADH)",
    "(NADP+)",
    "[acyl-carrier protein]",
    "[acyl-carrier-protein]",
    "(acyl carrier protein)"
};
static const string ok_num_prefix[] = {
    "DUF",
    "UPF",
    "IS",
    "TIGR",
    "UCP",
    "PUF",
    "CHP"
};


static bool IsStringConstraintEmpty(const CString_constraint* constraint)
{
    if (!constraint) return true;
    if (constraint->GetIs_all_caps() || constraint->GetIs_all_lower()|| constraint->GetIs_all_punct()) return false;
    if (!constraint->CanGetMatch_text() || constraint->GetMatch_text().empty()) return true;
    return false;
};


static const string SkipWeasel(const string& str)
{
    if (str.empty()) return kEmptyStr;
    string ret_str(kEmptyStr);
    vector <string> arr;
    arr = NStr::Tokenize(str, " ", arr);
    if (arr.size() == 1) return str;
    int i;
    unsigned len, len_w;
    bool find_w;
    for (i=0; i< (int)(arr.size() - 1); i++) {
        len = arr[i].size();
        find_w = false;
//        ITERATE (vector <string>, it, thisInfo.weasels) {
//            len_w = (*it).size(); 
//            if (len != len_w || !NStr::EqualNocase(arr[i], 0, len, *it)) continue;
//            else { 
//                find_w = true;
//                break;
//            }
//        }
        if (!find_w) break;
    }
    for ( ; i< (int)(arr.size()-1); i++) ret_str += arr[i] + ' ';
    ret_str += arr[arr.size()-1];
    return (ret_str);
}


static bool CaseNCompareEqual(string str1, string str2, unsigned len1, bool case_sensitive)
{
    if (!len1) return false;
    string comp_str1, comp_str2;
    comp_str1 = CTempString(str1).substr(0, len1);
    comp_str2 = CTempString(str2).substr(0, len1);
    if (case_sensitive) return (comp_str1 == comp_str2);
    else return (NStr::EqualNocase(comp_str1, 0, len1, comp_str2));
};


static bool AdvancedStringCompare(const string& str, const string& str_match, const CString_constraint* str_cons, bool is_start, unsigned* ini_target_match_len = 0)
{
    if (!str_cons) return true;

    size_t pos_match = 0, pos_str = 0;
    bool wd_case, whole_wd, word_start_m, word_start_s;
    bool match = true, recursive_match = false;
    unsigned len_m = str_match.size(), len_s = str.size(), target_match_len=0;
    string cp_m, cp_s;
    bool ig_space = str_cons->GetIgnore_space();
    bool ig_punct = str_cons->GetIgnore_punct();
    bool str_case = str_cons->GetCase_sensitive();
    EString_location loc = str_cons->GetMatch_location();
    unsigned len1, len2;
    char ch1, ch2;
    vector <string> word_word;
    bool has_word = !(str_cons->GetIgnore_words().Get().empty());
    string strtmp;
    ITERATE (list <CRef <CWord_substitution> >, it, str_cons->GetIgnore_words().Get()) {
        strtmp = ((*it)->CanGetWord()) ? (*it)->GetWord() : kEmptyStr;
        word_word.push_back(strtmp);
    }

    unsigned i;
    while (match && pos_match < len_m && pos_str < len_s && !recursive_match) {
    cp_m = CTempString(str_match).substr(pos_match);
    cp_s = CTempString(str).substr(pos_str);

    /* first, check to see if we're skipping synonyms */
    i=0;
    if (has_word) {
        ITERATE (list <CRef <CWord_substitution> >, it, str_cons->GetIgnore_words().Get()) {
            wd_case = (*it)->GetCase_sensitive();
            whole_wd = (*it)->GetWhole_word();
            len1 = word_word[i].size();
            //text match
            if (CaseNCompareEqual(word_word[i++], cp_m, len1, wd_case)){
                word_start_m = (!pos_match && is_start) || !isalpha(cp_m[pos_match-1]);
                ch1 = (cp_m.size() <= len1) ? ' ' : cp_m[len1];
           
                // whole word mch
                if (!whole_wd || (!isalpha(ch1) && word_start_m)) { 
                    if ( !(*it)->CanGetSynonyms() || (*it)->GetSynonyms().empty()) {
                        if (AdvancedStringCompare(cp_s, CTempString(cp_m).substr(len1), str_cons, word_start_m, &target_match_len)) {
                            recursive_match = true;
                            break;
                        }
                    }
                    else {
                        ITERATE (list <string>, sit, (*it)->GetSynonyms()) {
                            len2 = (*sit).size();

                            // text match
                            if (CaseNCompareEqual(*sit, cp_s, len2, wd_case)) {
                                word_start_s = (!pos_str && is_start) || !isalpha(cp_s[pos_str-1]);
                                ch2 = (cp_s.size() <= len2) ? ' ' : cp_s[len2];
                                // whole word match
                                if (!whole_wd || (!isalpha(ch2) && word_start_s)) {
                                    if (AdvancedStringCompare(CTempString(cp_s).substr(len2), CTempString(cp_m).substr(len1), str_cons, word_start_m & word_start_s, &target_match_len)) {
                                        recursive_match = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (!recursive_match) {
        if (CaseNCompareEqual(cp_m, cp_s, 1, str_case)) {
            pos_match++;
            pos_str++;
            target_match_len++;
        } 
        else if ( ig_space && (isspace(cp_m[0]) || isspace(cp_s[0])) ) {
            if (isspace(cp_m[0])) pos_match++;
            if (isspace(cp_s[0])) {
                pos_str++;
                target_match_len++;
            }
        }
        else if (ig_punct && ( ispunct(cp_m[0]) || ispunct(cp_s[0]) )) {
            if ( ispunct(cp_m[0]) ) pos_match++;
                if ( ispunct(cp_s[0]) ) {
                    pos_str++;
                    target_match_len++;
                }
            }
            else match = false;
        }
    }

    if (match && !recursive_match) {
        while ( pos_str < str.size() 
                    && ((ig_space && isspace(str[pos_str])) 
                        || (ig_punct && ispunct(str[pos_str]))) ){
            pos_str++;
            target_match_len++;
        }
        while (pos_match < str_match.size() && ((ig_space && isspace(str_match[pos_match])) || (ig_punct && ispunct(str_match[pos_match]))))  pos_match++;

        if (pos_match < str_match.size()) match = false;
        else if ((loc == eString_location_ends || loc == eString_location_equals) && pos_str < len_s) match = false;
        else if (str_cons->GetWhole_word() && (!is_start || (pos_str < len_s && isalpha (str[pos_str])))) match = false;
    }
    if (match && ini_target_match_len) *ini_target_match_len += target_match_len;
    return match;
}


static bool AdvancedStringMatch(const string& str, const CString_constraint* str_cons)
{
    if (!str_cons) return true;
    bool rval = false;
    string 
    match_text = (str_cons->CanGetMatch_text()) ? str_cons->GetMatch_text() : kEmptyStr;

    if (AdvancedStringCompare (str, match_text, str_cons, true)) return true;
    else if (str_cons->GetMatch_location() == eString_location_starts || str_cons->GetMatch_location() == eString_location_equals) return false;
    else {
        size_t pos = 1;
        unsigned len = str.size();
        while (!rval && pos < len) {
            if (str_cons->GetWhole_word()) {
                while (pos < len && isalpha (str[pos-1])) pos++;
            }
            if (pos < len) {
                if (AdvancedStringCompare (CTempString(str).substr(pos), match_text, str_cons, true)) rval = true;
                else pos++;
            }
        }
    }
    return rval;
}


static string StripUnimportantCharacters(const string& str, bool strip_space, bool strip_punct)
{
    if (str.empty()) return kEmptyStr;
    string result;
    result.reserve(str.size());
    string::const_iterator it = str.begin();
    do {
        if ((strip_space && isspace(*it)) || (strip_punct && ispunct(*it)));
        else result += *it;
    } while (++it != str.end());

    return result;
}


static bool DisallowCharacter(const char ch, bool disallow_slash)
{
    if (isalpha (ch) || isdigit (ch) || ch == '_' || ch == '-') return true;
    else if (disallow_slash && ch == '/') return true;
    else return false;
}


static bool IsWholeWordMatch(const string& start, const size_t& found, const unsigned& match_len, bool disallow_slash = false)
{
    bool rval = true;
    unsigned after_idx;

    if (!match_len) rval = true;
    else if (start.empty() || found == string::npos) rval = false;
    else
    {
        if (found) {
            if (DisallowCharacter (start[found-1], disallow_slash)) return false;
        }
        after_idx = found + match_len;
        if ( after_idx < start.size() && DisallowCharacter(start[after_idx], disallow_slash)) rval = false;
    }
    return rval;
}


static bool GetSpanFromHyphenInString(const string& str, const size_t& hyphen, string& first, string& second)
{
    if (!hyphen) return false;

    /* find range start */
    size_t cp = str.substr(0, hyphen-1).find_last_not_of(' ');   
    if (cp != string::npos) cp = str.substr(0, cp).find_last_not_of(" ,;"); 
    if (cp == string::npos) cp = 0;

    unsigned len = hyphen - cp;
    first = CTempString(str).substr(cp, len);
    NStr::TruncateSpacesInPlace(first);
 
    /* find range end */
    cp = str.find_first_not_of(' ', hyphen+1);
    if (cp != string::npos) cp = str.find_first_not_of(" ,;");
    if (cp == string::npos) cp = str.size() -1;   

    len = cp - hyphen;
    if (!isspace (str[cp])) len--;
    second = CTempString(str).substr(hyphen+1, len);
    NStr::TruncateSpacesInPlace(second);

    bool rval = true;
    if (first.empty() || second.empty()) rval = false;
    else if (!isdigit (first[first.size() - 1]) || !isdigit (second[second.size() - 1])) {
        /* if this is a span, then neither end point can end with anything other than a number */
        rval = false;
    }
    if (!rval) first = second = "";
    return rval;
}


static bool StringIsPositiveAllDigits(const string& str)
{
   if (str.find_first_not_of(digit_str) != string::npos) return false;
   return true;
};


static bool IsStringInSpan(const string& str, const string& first, const string& second)
{
    string new_first, new_second, new_str;
    if (str.empty()) return false;
    else if (str == first || str == second) return true;
    else if (first.empty() || second.empty()) return false;

    int str_num, first_num, second_num;
    str_num = first_num = second_num = 0;
    bool rval = false;
    size_t prefix_len;
    string comp_str1, comp_str2;
    if (StringIsPositiveAllDigits(first)) {
        if (StringIsPositiveAllDigits (str) && StringIsPositiveAllDigits (second)) {
            str_num = NStr::StringToUInt (str);
            first_num = NStr::StringToUInt (first);
            second_num = NStr::StringToUInt (second);
            if ((str_num > first_num && str_num < second_num) || (str_num > second_num && str_num < first_num)) rval = true;
        }
    } 
    else if (StringIsPositiveAllDigits(second)) {
        prefix_len = first.find_first_of(digit_str) + 1;

        new_str = CTempString(str).substr(prefix_len - 1);
        new_first = CTempString(first).substr(prefix_len - 1);
        comp_str1 = CTempString(str).substr(0, prefix_len);
        comp_str2 = CTempString(first).substr(0, prefix_len);
        if (comp_str1 == comp_str2 && StringIsPositiveAllDigits (new_str) && StringIsPositiveAllDigits (new_first)) {
            first_num = NStr::StringToUInt(new_first);
            second_num = NStr::StringToUInt (second);
            str_num = NStr::StringToUInt (str);
            if ((str_num > first_num && str_num < second_num) || (str_num > second_num && str_num < first_num)) rval = true;
        }
    } 
    else {
        /* determine length of prefix */
        prefix_len = 0;
        while (prefix_len < first.size() && prefix_len < second.size() && first[prefix_len] == second[prefix_len]) prefix_len ++;
        prefix_len ++;

        comp_str1 = CTempString(str).substr(0, prefix_len);
        comp_str2 = CTempString(first).substr(0, prefix_len);
        if (prefix_len <= first.size() && prefix_len <= second.size() && isdigit (first[prefix_len-1]) && isdigit (second[prefix_len-1]) && comp_str1 == comp_str2) {
            new_first = CTempString(first).substr(prefix_len);
            new_second = CTempString(second).substr(prefix_len);
            new_str = CTempString(str).substr(prefix_len);
            if (StringIsPositiveAllDigits (new_first) && StringIsPositiveAllDigits (new_second) && StringIsPositiveAllDigits (new_str)) {
                first_num = NStr::StringToUInt(new_first);
                second_num = NStr::StringToUInt (new_second);
                str_num = NStr::StringToUInt (new_str);
                if ((str_num > first_num && str_num < second_num) || (str_num > second_num && str_num < first_num)) rval = true;
            } else {
                /* determine whether there is a suffix */
                size_t idx1, idx2, idx_str;
                string suf1, suf2, sub_str;
                idx1 = first.find_first_not_of(digit_str);
                suf1 = CTempString(first).substr(prefix_len + idx1);
                idx2 = second.find_first_not_of(digit_str);
                suf2 = CTempString(second).substr(prefix_len + idx2);
                idx_str = str.find_first_not_of(digit_str);
                sub_str = CTempString(str).substr(prefix_len + idx_str);
                if (suf1 == suf2 && suf1 == sub_str) {
                    /* suffixes match */
                    first_num = NStr::StringToUInt(CTempString(first).substr(prefix_len, idx1));
                    second_num = NStr::StringToUInt(CTempString(second).substr(prefix_len, idx2));
                    str_num = NStr::StringToUInt(CTempString(str).substr(prefix_len, idx_str));
                    if ((str_num > first_num && str_num < second_num) || (str_num > second_num && str_num < first_num)) rval = true;
                }
            }
        }
    }
    return rval;
}


static bool IsStringInSpanInList(const string& str, const string& list)
{
    if (list.empty() || str.empty()) return false;

    size_t idx = str.find_first_not_of(alpha_str);
    if (idx == string::npos) return false;

    idx = CTempString(str).substr(idx).find_first_not_of(digit_str);

    /* find ranges */
    size_t hyphen = list.find('-');
    bool rval = false;
    string range_start, range_end;
    while (hyphen != string::npos && !rval) {
        if (!hyphen) hyphen = CTempString(list).substr(1).find('-');
        else {
            if (GetSpanFromHyphenInString (list, hyphen, range_start, range_end)) {
                if (IsStringInSpan (str, range_start, range_end)) rval = true;
            }
            hyphen = list.find('-', hyphen + 1);
        }
    }
    return rval;
}


static bool DoesSingleStringMatchConstraint(const string& str, const CString_constraint* str_cons)
{
    bool rval = false;
    string tmp_match;
    CString_constraint tmp_cons;

    string this_str(str);
    if (!str_cons) return true;
    if (str.empty()) return false;
    if (IsStringConstraintEmpty(str_cons)) rval = true;
    else {
        if (str_cons->GetIgnore_weasel()) {
            this_str = SkipWeasel(str);
        }
        if (str_cons->GetIs_all_caps() && !IsAllCaps(this_str)) {
            rval = false;
        }
        else if (str_cons->GetIs_all_lower() && !IsAllLowerCase(this_str)) {
            rval = false;
        }
        else if (str_cons->GetIs_all_punct() && !IsAllPunctuation(this_str)) {
            rval = false;
        }
        else if (!str_cons->CanGetMatch_text() ||str_cons->GetMatch_text().empty()) {
            rval = true; 
        }
        else {
            /**
            string strtmp = Blob2Str(*str_cons);
            Str2Blob(strtmp, tmp_cons);
            **/
            tmp_cons.Assign(*str_cons);
            tmp_match = tmp_cons.CanGetMatch_text() ? tmp_cons.GetMatch_text() : kEmptyStr;
            if (str_cons->GetIgnore_weasel()) {
            tmp_cons.SetMatch_text(SkipWeasel(str_cons->GetMatch_text()));
        }
        if ((str_cons->GetMatch_location() != eString_location_inlist) && str_cons->CanGetIgnore_words()){
            tmp_cons.SetMatch_text(tmp_match);
            rval = AdvancedStringMatch(str, &tmp_cons);
        }
        else {
            string search(this_str), pattern(tmp_cons.GetMatch_text());
            bool ig_space = str_cons->GetIgnore_space();
            bool ig_punct = str_cons->GetIgnore_punct();
            if ( (str_cons->GetMatch_location() != eString_location_inlist) && (ig_space || ig_punct)) {
                search = StripUnimportantCharacters(search, ig_space, ig_punct);
                pattern = StripUnimportantCharacters(pattern, ig_space, ig_punct);
            } 

            size_t pFound;
            pFound = (str_cons->GetCase_sensitive())?
            search.find(pattern) : NStr::FindNoCase(search, pattern);
            switch (str_cons->GetMatch_location()) 
            {
                case eString_location_contains:
                    if (string::npos == pFound) rval = false;
                    else if (str_cons->GetWhole_word()) {
                        rval = IsWholeWordMatch (search, pFound, pattern.size());
                        while (!rval && pFound != string::npos) {
                            pFound = (str_cons->GetCase_sensitive()) ?
                            search.find(pattern, pFound+1):
                            NStr::FindNoCase(search, pattern, pFound+1);
                            rval = (pFound != string::npos)? 
                            IsWholeWordMatch (search, pFound, pattern.size()):
                            false;
                        }
                    }
                    else  rval = true;
                    break;
                case eString_location_starts:
                    if (!pFound) rval = (str_cons->GetWhole_word()) ? IsWholeWordMatch (search, pFound, pattern.size()) : true;
                    break;
                case eString_location_ends:
                    while (pFound != string::npos && !rval) {
                        if ( (pFound + pattern.size()) == search.size()) {
                            rval = (str_cons->GetWhole_word())? 
                            IsWholeWordMatch (search, pFound, pattern.size()): true;
                            /* stop the search, we're at the end of the string */
                            pFound = string::npos;
                        }
                        else {
                            if (pattern.empty()) pFound = false;
                            else pFound = (str_cons->GetCase_sensitive()) ? pFound = search.find(pattern, pFound+1) : NStr::FindNoCase(search, pattern, pFound+1);
                        }
                    }
                    break;
                case eString_location_equals:
                    if (str_cons->GetCase_sensitive() && search==pattern) rval= true; 
                    else if (!str_cons->GetCase_sensitive() && NStr::EqualNocase(search, pattern)) rval = true;
                    break;
                case eString_location_inlist:
                    pFound = (str_cons->GetCase_sensitive())?
                    pattern.find(search) : NStr::FindNoCase(pattern, search);
                    if (pFound == string::npos) rval = false;
                    else {
                        rval = IsWholeWordMatch(pattern, pFound, search.size(), true);
                        while (!rval && pFound != string::npos) {
                            pFound = (str_cons->GetCase_sensitive()) ? CTempString(pattern).substr(pFound + 1).find(search) : NStr::FindNoCase(CTempString(pattern).substr(pFound + 1), search);
                            if (pFound != string::npos) rval = IsWholeWordMatch(pattern, pFound, str.size(), true);
                        }
                    }
                    if (!rval) {
                        /* look for spans */
                        rval = IsStringInSpanInList (search, pattern);
                    }
                    break;
                }
            }
        }
    }
    return rval;
}


static bool DoesStringMatchConstraint(const string& str, const CString_constraint* constraint)
{
    if (!constraint) return true;
    bool rval = DoesSingleStringMatchConstraint (str, constraint);
    if (constraint->GetNot_present()) return (!rval);
    else return rval;
}


static bool DoesStrContainPlural(const string& word, char last_letter, char second_to_last_letter, char next_letter)
{
    unsigned len = word.size();
    if (last_letter == 's') {
        if (len >= 5  && CTempString(word).substr(len-5) == "trans") {
            return false; // not plural;
        }
        else if (len > 3) {
            if (second_to_last_letter != 's' && second_to_last_letter != 'i' && second_to_last_letter != 'u' && next_letter == ',') return true;
        }
    }
    return false;
}


static bool StringMayContainPlural(const string& str)
{
    char last_letter, second_to_last_letter, next_letter;
    bool may_contain_plural = false;
    string word_skip = " ,";
    unsigned len;

    if (str.empty()) return false;
    vector <string> arr;
    arr = NStr::Tokenize(str, " ,", arr, NStr::eMergeDelims);
    if (arr.size() == 1) { // doesn't have ', ', or the last char is ', '
        len = arr[0].size();
        if (len == 1) return false;
        last_letter = arr[0][len-1];
        second_to_last_letter = arr[0][len-2]; 
        if (len == str.size()) next_letter = ',';
        else next_letter = str[len];
        may_contain_plural = DoesStrContainPlural(arr[0], last_letter, second_to_last_letter, next_letter);
    }
    else {
        string strtmp = str;
        size_t pos;
        vector <string>::const_iterator jt;
        ITERATE (vector <string>, it, arr) { 
            pos = strtmp.find(*it);
            len = (*it).size();
            if (len != 1) {
                last_letter = (*it)[len-1];
                second_to_last_letter = (*it)[len-2];
                if (len == strtmp.size()) next_letter = ',';
                else next_letter = strtmp[pos+len];
                may_contain_plural = DoesStrContainPlural(*it, last_letter, second_to_last_letter, next_letter);
                if (may_contain_plural) break;
            }
            jt = it;
            if (++jt != arr.end()) { // not jt++
                strtmp = CTempString(strtmp).substr(strtmp.find(*jt));
            }
        }
    }
    return may_contain_plural;
}


static char GetClose(char bp)
{
    if (bp == '(') return ')';
    else if (bp == '[') return ']';
    else if (bp == '{') return '}';
    else return bp;
}


static bool SkipBracketOrParen(const unsigned& idx, string& start)
{
    bool rval = false;
    size_t ep, ns;
    string strtmp;

    if (idx > 2 && CTempString(start).substr(idx-3, 6) == "NAD(P)") {
        rval = true;
        start = CTempString(start).substr(idx + 3);
    } 
    else {
        unsigned len;
        for (unsigned i=0; i< ArraySize(skip_bracket_paren); i++) {
            strtmp = skip_bracket_paren[i];
            len = strtmp.size();
            if (CTempString(start).substr(idx, len) == strtmp) {
                start = CTempString(start).substr(idx + len);
                rval = true;
                break;
            }
        }
        if (!rval) {
            ns = start.find(start[idx], idx+1);
            ep = start.find(GetClose(start[idx]), idx+1);
            if (ep != string::npos && (ns == string::npos || ns > ep)) {
                if (ep - idx < 5) {
                    rval = true;
                    start = CTempString(start).substr(ep+1);
                } 
                else if (ep - idx > 3 && CTempString(start).substr(ep - 3, 3) == "ing") {
                    rval = true;
                    start = CTempString(start).substr(ep + 1);
                }
            }
        }
    }
    return rval;
}


static bool ContainsNorMoreSetsOfBracketsOrParentheses(const string& search, const int& n)
{
    size_t idx, end;
    int num_found = 0;
    string open_bp("(["), sch_src(search);

    if (sch_src.empty()) return false;

    idx = sch_src.find_first_of(open_bp);
    while (idx != string::npos && num_found < n) {
        end = sch_src.find(GetClose(sch_src[idx]), idx);
        if (SkipBracketOrParen (idx, sch_src)) { // ignore it
            idx = sch_src.find_first_of(open_bp);
        }
        else if (end == string::npos) { // skip, doesn't close the bracket
            idx = sch_src.find_first_of(open_bp, idx+1);
        }
        else {
            idx = sch_src.find_first_of(open_bp, end);
            num_found++;
        }
    }
 
    if (num_found >= n) return true;
    else return false;
}


static bool PrecededByOkPrefix (const string& start_str)
{
    unsigned len_str = start_str.size();
    unsigned len_it;
    for (unsigned i=0; i< ArraySize(ok_num_prefix); i++) {
        len_it = ok_num_prefix[i].size();
        if (len_str >= len_it && (CTempString(start_str).substr(len_str-len_it) == ok_num_prefix[i])) return true;
    }
    return false;
}


static bool InWordBeforeCytochromeOrCoenzyme(const string& start_str)
{
    if (start_str.empty()) return false;
    size_t pos = start_str.find_last_of(' ');
    string comp_str1, comp_str2;
    if (pos != string::npos) {
        string strtmp = CTempString(start_str).substr(0, pos);
        pos = strtmp.find_last_not_of(' ');
        if (pos != string::npos) {
            unsigned len = strtmp.size();
            comp_str1 = CTempString(strtmp).substr(len-10);
            comp_str2 = CTempString(strtmp).substr(len-8);
            if ( (len >= 10  && NStr::EqualNocase(comp_str1, "cytochrome")) || (len >= 8 && NStr::EqualNocase(comp_str2, "coenzyme"))) return true;
        }
    }
    return false;
}


static bool FollowedByFamily(string& after_str)
{
    size_t pos = after_str.find_first_of(' ');
    if (pos != string::npos) {
        after_str = CTempString(after_str).substr(pos+1);
        if (NStr::EqualNocase(after_str, 0, 6, "family")) {
            after_str = CTempString(after_str).substr(7);
            return true;
        }
    } 
    return false;
}


static bool ContainsThreeOrMoreNumbersTogether(const string& search)
{
    size_t p=0, p2;
    unsigned num_digits = 0;
    string sch_str = search;
  
    while (!sch_str.empty()) {
        p = sch_str.find_first_of(digit_str);
        if (p == string::npos) break;
        if (p && (PrecededByOkPrefix(CTempString(sch_str).substr(0, p)) || InWordBeforeCytochromeOrCoenzyme (CTempString(sch_str).substr(0, p)))) {
            p2 = sch_str.find_first_not_of(digit_str, p+1);
            if (p2 != string::npos) {
                sch_str = CTempString(sch_str).substr(p2);
                num_digits = 0;
            }
            else break;
        }
        else {
            num_digits ++;
            if (num_digits == 3) {
                sch_str = CTempString(sch_str).substr(p+1);
                if (FollowedByFamily(sch_str)) num_digits = 0;
                else return true;
            }
            if (p < sch_str.size() - 1) {
                sch_str = CTempString(sch_str).substr(p+1);
                if (!isdigit(sch_str[0])) num_digits = 0;
            }
            else break;
        }
    }
    return false;
}


static bool StringContainsUnderscore(const string& search)
{ 
    if (search.find('_') == string::npos) return false;

    string strtmp;
    vector <string> arr;
    arr = NStr::Tokenize(search, "_", arr);
    for (unsigned i=0; i< arr.size() - 1; i++) {
        strtmp = arr[i+1];
        if (FollowedByFamily(strtmp)) continue; // strtmp was changed in the FollowedByFamily
        else if (arr[i].size() < 3 || search[arr[i].size()-1] == ' ') return true;
        else {
            strtmp = CTempString(arr[i]).substr(arr[i].size()-3);
            if ((strtmp == "MFS" || strtmp == "TPR" || strtmp == "AAA") && (isdigit(arr[i+1][0]) && !isdigit(arr[i+1][1]))) continue;
            else return true;
        }
    }
    return false;
}


static bool IsPrefixPlusNumbers(const string& prefix, const string& search)
{
    unsigned pattern_len;
    pattern_len = prefix.size();
    if (pattern_len > 0 && !NStr::EqualCase(search, 0, pattern_len, prefix)) {
        return false;
    }
    size_t digit_len = search.find_first_not_of(digit_str, pattern_len);
    if (digit_len != string::npos && digit_len == search.size()) {
        return true;
    }
    else return false;
}


static bool IsPropClose(const string& str, char open_p)
{
    if (str.empty()) return false;
    else if (str[str.size()-1] != open_p) return false;
    else return true;
}


static bool StringContainsUnbalancedParentheses(const string& search)
{
    size_t pos = 0;
    char ch_src;
    string strtmp = kEmptyStr, sch_src(search);
    while (!sch_src.empty()) {
    pos = sch_src.find_first_of("()[]");
    if (pos == string::npos) {
        if (strtmp.empty()) return false;
        else return true;
    }
    else {
        ch_src = sch_src[pos];
        if (ch_src == '(' || ch_src == '[') {
            strtmp += ch_src;
        }
        else if (sch_src[pos] == ')') {
            if (!IsPropClose(strtmp, '(')) return true;
            else strtmp = strtmp.substr(0, strtmp.size()-1);
        }
        else if (sch_src[pos] == ']') {
            if (!IsPropClose(strtmp, '[')) return true;
            else strtmp = strtmp.substr(0, strtmp.size()-1);
        }
    }
    if (pos < sch_src.size()-1) {
        sch_src = sch_src.substr(pos+1);
    }
    else sch_src = kEmptyStr;
    }
  
    if (strtmp.empty()) return false;
    else return true;
}


static bool ProductContainsTerm(const string& pattern, const string& search)
{
    /* don't bother searching for c-term or n-term if product name contains "domain" */
    if (NStr::FindNoCase(search, "domain") != string::npos) return false;

    size_t pos = NStr::FindNoCase(search, pattern);
    /* c-term and n-term must be either first word or separated from other word by space, num, or punct */
    if (pos != string::npos && (!pos || !isalpha (search[pos-1]))) return true;
    else return false;
}


static bool MatchesSearchFunc(const string& str, const CSearch_func& func)
{
    switch (func.Which()){
        case CSearch_func::e_String_constraint:
            return DoesStringMatchConstraint(str, &(func.GetString_constraint()));
        case CSearch_func::e_Contains_plural:
            return StringMayContainPlural(str);
        case  CSearch_func::e_N_or_more_brackets_or_parentheses:
            return ContainsNorMoreSetsOfBracketsOrParentheses(str, func.GetN_or_more_brackets_or_parentheses());
        case CSearch_func::e_Three_numbers:
            return ContainsThreeOrMoreNumbersTogether (str);
        case CSearch_func::e_Underscore:
            return StringContainsUnderscore(str);
        case CSearch_func::e_Prefix_and_numbers:
            return IsPrefixPlusNumbers(func.GetPrefix_and_numbers(), str);
        case CSearch_func::e_All_caps:
            return IsAllCaps(str);
        case CSearch_func::e_Unbalanced_paren:
            return StringContainsUnbalancedParentheses(str);
        case CSearch_func::e_Too_long:
            return NStr::FindNoCase(str, "bifunctional") == string::npos && NStr::FindNoCase(str, "multifunctional") == string::npos && str.size() > (unsigned) func.GetToo_long();
        case CSearch_func::e_Has_term:  
            return ProductContainsTerm(func.GetHas_term(), str);
        default: break;
    }
    return false; 
};


static bool IsSearchFuncEmpty(const CSearch_func& func)
{
    switch (func.Which()) {
        case CSearch_func::e_String_constraint:
            return IsStringConstraintEmpty(&(func.GetString_constraint()));
        case  CSearch_func::e_Prefix_and_numbers:
            return func.GetPrefix_and_numbers().empty();
        default: return false;
    }
    return false;
}

/*
static bool DoesObjectMatchStringConstraint(const CSeq_feat& feat, const vector <string>& strs, const CString_constraint& str_cons)
{
    bool rval = false;
    ITERATE (vector <string>, it, strs) { 
        rval = DoesSingleStringMatchConstraint(*it, &str_cons); 
        if (rval) {
            break;
        }
    }
    if (!rval) {
        string str(kEmptyStr);
        switch (feat.GetData().Which()) {
        case CSeqFeatData::e_Cdregion: 
            if (feat.CanGetProduct()) {
                CBioseq_Handle bioseq_hl = GetBioseqFromSeqLoc(feat.GetProduct(), *thisInfo.scope);
                if (bioseq_hl) {
                    CFeat_CI ci(bioseq_hl, SAnnotSelector(CSeqFeatData::eSubtype_prot)); 
                    if (ci) {
                        vector <string> arr;
                        GetStringsFromObject(ci->GetOriginalFeature(), arr);
                        ITERATE (vector <string>, it, arr) {
                            rval = DoesSingleStringMatchConstraint(*it, &str_cons);
                            if (rval) break;
                        }
                    }
                }
            }
            break;
        case CSeqFeatData::e_Rna:
            if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_rRNA) {
                GetSeqFeatLabel(feat, str);
                rval = DoesSingleStringMatchConstraint(str, &str_cons);
                if (!rval) {
                str = "tRNA-" + str;
                rval = DoesSingleStringMatchConstraint(str, &str_cons);
                }
            }
            break;
        case CSeqFeatData::e_Imp:
            rval = DoesSingleStringMatchConstraint(feat.GetData().GetImp().GetKey(), &str_cons);
            break;
        default: break;
        }
    }
    if (str_cons.GetNot_present()) rval = !rval;
    return rval;
}


static bool DoesObjectMatchConstraint(const CSeq_feat& data, const vector <string>& strs, const CConstraint_choice& cons)
{
    switch (cons.Which()) {
    case CConstraint_choice::e_String :
        return DoesObjectMatchStringConstraint (data, strs, cons.GetString());
    case CConstraint_choice::e_Location :
        return DoesFeatureMatchLocationConstraint (data, cons.GetLocation());
    case CConstraint_choice::e_Field :
        return DoesObjectMatchFieldConstraint (data, cons.GetField());
    case CConstraint_choice::e_Source :
        if (data.GetData().IsBiosrc()) {
            return DoesBiosourceMatchConstraint ( data.GetData().GetBiosrc(), 
                                                cons.GetSource());
        }
        else {
            CBioseq_Handle 
                seq_hl = sequence::GetBioseqFromSeqLoc(data.GetLocation(), 
                                                    *thisInfo.scope);
            const CBioSource* src = GetBioSource(seq_hl);
            if (src) {
            return DoesBiosourceMatchConstraint(*src, cons.GetSource());
            }
            else return false;
        }
        break;
    case CConstraint_choice::e_Cdsgeneprot_qual :
        return DoesFeatureMatchCGPQualConstraint (data, 
                                                    cons.GetCdsgeneprot_qual());
    case CConstraint_choice::e_Cdsgeneprot_pseudo :
        return DoesFeatureMatchCGPPseudoConstraint (data, cons.GetCdsgeneprot_pseudo());
    case CConstraint_choice::e_Sequence :
        {
        if (m_bioseq_hl) 
            return DoesSequenceMatchSequenceConstraint(cons.GetSequence());
    //      rval = DoesObjectMatchSequenceConstraint (choice, data, cons->data.ptrvalue);
        break;
        }
    case CConstraint_choice::e_Pub:
        if (data.GetData().IsPub())
            return DoesPubMatchPublicationConstraint(data.GetData().GetPub(), cons.GetPub());
        break;
    case CConstraint_choice::e_Molinfo:
        return DoesObjectMatchMolinfoFieldConstraint (data, cons.GetMolinfo()); // use bioseq_hl
    case CConstraint_choice::e_Field_missing:
        if (GetConstraintFieldFromObject(data, cons.GetField_missing()).empty()) 
                return true; 
        else return false;
        // return DoesObjectMatchFieldMissingConstraint (data, cons.GetField_missing());
    case CConstraint_choice::e_Translation:
        // must be coding region or protein feature
        if (data.GetData().IsProt()) {
            const CSeq_feat* cds = sequence::GetCDSForProduct(m_bioseq_hl);
            if (cds)
                return DoesCodingRegionMatchTranslationConstraint (*cds, cons.GetTranslation());
        }
        else if (data.GetData().IsCdregion())
            return DoesCodingRegionMatchTranslationConstraint(data, cons.GetTranslation());
        // DoesObjectMatchTranslationConstraint (data, cons.GetTrandlation());
    default: break;
    }
    return true;
}


static bool DoesObjectMatchConstraintChoiceSet(const CSeq_feat& feat, const CConstraint_choice_set& c_set)
{
    vector <string> strs_in_feat;
    GetStringsFromObject(feat, strs_in_feat);
    ITERATE (list <CRef <CConstraint_choice> >, sit, c_set.Get()) {
        if (!DoesObjectMatchConstraint(feat, strs_in_feat, **sit)) {
            return false;
        }
    }
    return true;
}
*/


static string GetRuleText(const CSuspect_rule& rule)
{
    return "...";
}


///////////////////////////////////// SUSPECT_PRODUCT_NAMES

DISCREPANCY_CASE(SUSPECT_PRODUCT_NAMES, CSeqFeatData)
{
    if (obj->GetSubtype() != CSeqFeatData::eSubtype_prot) return;
    CConstRef<CSuspect_rule_set> rules = context.GetProductRules();
    const CProt_ref& prot = obj->GetProt();
    string prot_name = *(prot.GetName().begin()); 

    ITERATE (list <CRef <CSuspect_rule> >, rule, rules->Get()) {
        const CSearch_func& find = (*rule)->GetFind();
        if (!MatchesSearchFunc(prot_name, find)) continue;
        if ((*rule)->CanGetExcept()) {
            const CSearch_func& except = (*rule)->GetExcept();
            if (!IsSearchFuncEmpty(except) && MatchesSearchFunc(prot_name, except)) continue;
        }
        if ((*rule)->CanGetFeat_constraint()) {
            const CConstraint_choice_set& constr = (*rule)->GetFeat_constraint();
            //if (!DoesObjectMatchConstraintChoiceSet(*context.GetCurrentSeq_feat(), constr)) continue;
        }
        CRef<CDiscrepancyObject> r(new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), false));
        Add("", r);

cout << "found! " << GetRuleText(**rule) << "\t" << prot_name << "\n";
    }
}


DISCREPANCY_SUMMARIZE(SUSPECT_PRODUCT_NAMES)
{
    CRef<CDiscrepancyItem> item(new CDiscrepancyItem(GetName(), "it works!"));
    item->SetDetails(m_Objs[""]);
    AddItem(CRef<CReportItem>(item.Release()));

/*
    CNcbiOstrstream ss;
    ss << GetName() << ": " << m_Objs.size() << " nucleotide Bioseq" << (m_Objs.size()==1 ? " is" : "s are") << " present";
    CRef<CDiscrepancyItem> item(new CDiscrepancyItem(CNcbiOstrstreamToString(ss)));
    item->SetDetails(m_Objs);
    AddItem(CRef<CReportItem>(item.Release()));
*/
}


DISCREPANCY_AUTOFIX(SUSPECT_PRODUCT_NAMES)
{
    return false;
}


///////////////////////////////////// TEST_ORGANELLE_PRODUCTS


DISCREPANCY_CASE(TEST_ORGANELLE_PRODUCTS, CSeqFeatData)
{
    if (obj->GetSubtype() != CSeqFeatData::eSubtype_prot) return;
    CConstRef<CSuspect_rule_set> rules = context.GetOrganelleProductRules();
    const CProt_ref& prot = obj->GetProt();
    string prot_name = *(prot.GetName().begin()); 

    ITERATE (list <CRef <CSuspect_rule> >, rule, rules->Get()) {
        const CSearch_func& find = (*rule)->GetFind();
        if (!MatchesSearchFunc(prot_name, find)) continue;
        if ((*rule)->CanGetExcept()) {
            const CSearch_func& except = (*rule)->GetExcept();
            if (!IsSearchFuncEmpty(except) && MatchesSearchFunc(prot_name, except)) continue;
        }
        if ((*rule)->CanGetFeat_constraint()) {
            const CConstraint_choice_set& constr = (*rule)->GetFeat_constraint();
            //if (!DoesObjectMatchConstraintChoiceSet(*context.GetCurrentSeq_feat(), constr)) continue;
        }
        CRef<CDiscrepancyObject> r(new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), false));
        Add("", r);

cout << "found! " << GetRuleText(**rule) << "\t" << prot_name << "\n";
    }
}


DISCREPANCY_SUMMARIZE(TEST_ORGANELLE_PRODUCTS)
{
    CRef<CDiscrepancyItem> item(new CDiscrepancyItem(GetName(), "it works!"));
    item->SetDetails(m_Objs[""]);
    AddItem(CRef<CReportItem>(item.Release()));

/*
    CNcbiOstrstream ss;
    ss << GetName() << ": " << m_Objs.size() << " nucleotide Bioseq" << (m_Objs.size()==1 ? " is" : "s are") << " present";
    CRef<CDiscrepancyItem> item(new CDiscrepancyItem(CNcbiOstrstreamToString(ss)));
    item->SetDetails(m_Objs);
    AddItem(CRef<CReportItem>(item.Release()));
*/
}


DISCREPANCY_AUTOFIX(TEST_ORGANELLE_PRODUCTS)
{
    return false;
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
