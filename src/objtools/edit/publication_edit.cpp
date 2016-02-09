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
* Author:  Igor Filippov, Andrea Asztalos
*
* File Description:
*   
*/

#include <ncbi_pch.hpp>
#include <objects/general/Name_std.hpp>
#include <objtools/edit/publication_edit.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

string GetFirstInitial(string input, bool skip_rest)
{
    char letter;
    string inits;
    string::iterator p = input.begin();

    // skip leading punct
    while (p != input.end() && !isalpha(*p))
        p++;

    while (p != input.end()) {
        if (p != input.end() && isalpha(*p)) {
            letter = *p;
            inits += toupper(letter);
            inits += '.';
            p++;
        }

        // skip rest of name
        if (skip_rest) {
            while (p != input.end() && isalpha(*p))
                p++;
        }
        else { // skip only subsequent low case letters
            while (p != input.end() && isalpha(*p) && *p == tolower(*p))
                p++;
        }

        bool dash = false;
        while (p != input.end() && !isalpha(*p)) {
            if (*p == '-')
                dash = true;
            p++;
        }
        if (dash)
            inits += '-';
    }
    return NStr::ToUpper(inits);
}

bool GenerateInitials(CName_std& name)
{
    string first_init;
    if (name.IsSetFirst()) {
        string first = name.GetFirst();
        first_init = GetFirstInitial(first, true);
    }

    string original_init = (name.IsSetInitials()) ? name.GetInitials() : kEmptyStr;
    if (!NStr::IsBlank(original_init)) {
        if (NStr::IsBlank(first_init)) {
            first_init += ".";
        }
        first_init += original_init;
    }
    if (first_init.empty()) {
        return false;
    }

    name.SetInitials(first_init);
    FixInitials(name);
    return true;
}

bool FixInitials(CName_std& name)
{
    if (!name.IsSetInitials())
        return false;

    string first_init;
    if (name.IsSetFirst()) {
        string first = name.GetFirst();
        first_init = GetFirstInitial(first, true);
    }

    string original_init = name.GetInitials();
    string middle_init = GetFirstInitial(original_init, false);

    if (!NStr::IsBlank(first_init) && NStr::StartsWith(middle_init, first_init, NStr::eNocase)) {
        middle_init = middle_init.substr(first_init.length());
    }

    string init(first_init);
    if (!NStr::IsBlank(middle_init)) {
        init.append(middle_init);
    }
    if (!NStr::IsBlank(init) && init != original_init) {
        name.SetInitials(init);
        return true;
    }
    return false;
}

bool MoveMiddleToFirst(CName_std& name)
{
    if (!name.IsSetInitials())
        return false;

    string initials = name.GetInitials();
    string first = (name.IsSetFirst()) ? name.GetFirst() : kEmptyStr;
    SIZE_TYPE dot = NStr::FindNoCase(initials, ".");
    if (dot != NPOS) {
        SIZE_TYPE cp = dot;
        while (isalpha((unsigned char)initials[++cp])) {}
        string middle = initials.substr(++dot, cp - 2);
        if (middle.size() > 1) {
            name.SetFirst(first + " " + middle);
            return true;
        }
    }
    return false;
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
