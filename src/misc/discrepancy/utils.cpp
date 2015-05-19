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
#include "utils.hpp"

BEGIN_NCBI_SCOPE;
BEGIN_SCOPE(NDiscrepancy)

const string alpha_str = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
const string digit_str = "0123456789";


bool IsAllCaps(const string& str)
{
    string up_str = str;
    if (up_str.find_first_not_of(alpha_str) != string::npos) return false;
    up_str = NStr::ToUpper(up_str);
    if (up_str == str) return true;
    else return false;
}


bool IsAllLowerCase(const string& str)
{
    string low_str = str;
    if (low_str.find_first_not_of(alpha_str) != string::npos) return false;
    low_str = NStr::ToLower(low_str);
    if (low_str == str) return true;
    else return false;
}


bool IsAllPunctuation(const string& str)
{
    for (unsigned i=0; i< str.size(); i++) {
        if (!ispunct(str[i])) return false;
    }
    return true;
}


bool IsWholeWord(const string& str, const string& phrase)
{
    if (str == phrase) return true;
    size_t pos = str.find(phrase);
    if (string::npos == pos) return false;
    size_t pos2 = pos+phrase.size();
    if ((!pos || !isalnum(str[pos-1])) && ((str.size() == pos2) || !isalnum(str[pos2]))) return true;
    else return false;
}


string GetTwoFieldSubfield(const string& str, unsigned subfield)
{
    string strtmp;
    if (str.empty() || subfield > 2)  return "";
    if (!subfield) return str;
    else {
        size_t pos = str.find(':');
        if (pos == string::npos) {
            if (subfield == 1) return str;
            else return kEmptyStr;
        }
        else {
            if (subfield == 1) return str.substr(0, pos);
            else {
                strtmp = CTempString(str).substr(pos+1).empty();
                if (!strtmp.empty()) return strtmp;
                else return "";
            }
        }
    }
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
