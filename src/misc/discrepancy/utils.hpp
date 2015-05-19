/*  $Id$
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
 * Authors:  Sema
 * Created:  05/04/2015
 */

#ifndef _MISC_DISCREPANCY_UTILS_H_
#define _MISC_DISCREPANCY_UTILS_H_

#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)

extern const string alpha_str;
extern const string digit_str;

bool IsWholeWord(const string& str, const string& phrase);
bool IsAllCaps(const string& str);
bool IsAllLowerCase(const string& str);
bool IsAllPunctuation(const string& str);
string GetTwoFieldSubfield(const string& str, unsigned subfield);


template <class T> void GetStringsFromObject(const T& obj, vector <string>& strs)
{
    CTypesConstIterator it(CStdTypeInfo<string>::GetTypeInfo(), CStdTypeInfo<utf8_string_type>::GetTypeInfo());
    for (it = ConstBegin(obj);  it;  ++it) strs.push_back(*static_cast<const string*>(it.GetFoundPtr()));
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE

#endif  // _MISC_DISCREPANCY_UTILS_H_
